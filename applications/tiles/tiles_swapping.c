/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2012
 *					All rights reserved
 *
 *  This file is part of GPAC / command-line mp4 toolbox
 *
 *  GPAC is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  GPAC is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <gpac/bitstream.h>
#include <gpac/internal/media_dev.h>


static u8 avc_golomb_bits[256] = {
	8, 7, 6, 6, 5, 5, 5, 5, 4, 4, 4, 4, 4, 4, 4, 4, 3,
	3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 2, 2,
	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0
};

static u32 bs_get_ue(GF_BitStream *bs)
{
	u8 coded;
	u32 bits = 0, read = 0;
	while (1) {
		read = gf_bs_peek_bits(bs, 8, 0);
		if (read) break;
		//check whether we still have bits once the peek is done since we may have less than 8 bits available
		if (!gf_bs_available(bs)) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CODING, ("[AVC/HEVC] Not enough bits in bitstream !!\n"));
			return 0;
		}
		gf_bs_read_int(bs, 8);
		bits += 8;
	}
	coded = avc_golomb_bits[read];
	gf_bs_read_int(bs, coded);
	bits += coded;
	return gf_bs_read_int(bs, bits + 1) - 1;
}

static s32 bs_get_se(GF_BitStream *bs)
{
	u32 v = bs_get_ue(bs);
	if ((v & 0x1) == 0) return (s32) (0 - (v>>1));
	return (v + 1) >> 1;
}

void bs_set_ue(GF_BitStream *bs,s32 num) 
{
    	s32 length = 1;
    	s32 temp = ++num;

    	while (temp != 1) 
	{
        	temp >>= 1;
        	length += 2;
    	}
    
    	gf_bs_write_int(bs, 0, length >> 1);
    	gf_bs_write_int(bs, num, (length+1) >> 1);
}


	int flag_1=0;
	int flag_2=0;
void rewrite_slice_address(u32 new_address, char *in_slice, u32 in_slice_length, char **out_slice, u32 *out_slice_length, HEVCState* hevc) 
{
	u64 length_no_use = 0;
	GF_BitStream *bs_ori = gf_bs_new(in_slice, in_slice_length+1, GF_BITSTREAM_READ);
	GF_BitStream *bs_rw = gf_bs_new(NULL, length_no_use, GF_BITSTREAM_WRITE);
	u32 first_slice_segment_in_pic_flag;
	u32 dependent_slice_segment_flag;
	int address_ori;	

	/*HEVCState hevc;
	HEVCSliceInfo si;
	u8 nal_unit_type, temporal_id, layer_id;
	gf_media_hevc_parse_nalu(bs_rw, &hevc, &nal_unit_type, &temporal_id, &layer_id);*/


	u32 F = gf_bs_read_int(bs_ori, 1);			 //nal_unit_header
	gf_bs_write_int(bs_rw, F, 1);

	u32 nal_unit_type = gf_bs_read_int(bs_ori, 6);
	gf_bs_write_int(bs_rw, nal_unit_type, 6);
	u32 rest_nalu_header = gf_bs_read_int(bs_ori, 9);
	gf_bs_write_int(bs_rw, rest_nalu_header, 9);


	first_slice_segment_in_pic_flag = gf_bs_read_int(bs_ori, 1);    //first_slice_segment_in_pic_flag
	if (new_address == 0)  					
	{	
		gf_bs_write_int(bs_rw, 1, 1);
	}
	else
	{
		gf_bs_write_int(bs_rw, 0, 1);	
	}
	
 
	if (nal_unit_type >= 16 && nal_unit_type <= 23)                 //no_output_of_prior_pics_flag
	{
		u32 no_output_of_prior_pics_flag = gf_bs_read_int(bs_ori, 1);    
		gf_bs_write_int(bs_rw, no_output_of_prior_pics_flag, 1);
	}
	else;

	u32 pps_id = bs_get_ue(bs_ori);					 //pps_id
	bs_set_ue(bs_rw, pps_id);

	HEVC_PPS *pps;
	HEVC_SPS *sps;
	pps = &hevc->pps[pps_id];
	sps = &hevc->sps[pps->sps_id];

	if (!first_slice_segment_in_pic_flag && pps->dependent_slice_segments_enabled_flag) //dependent_slice_segment_flag READ
	{ 
		dependent_slice_segment_flag = gf_bs_read_int(bs_ori, 1);
	}
	else;
	if (!first_slice_segment_in_pic_flag) 						    //slice_segment_address READ
	{
		address_ori = gf_bs_read_int(bs_ori, sps->bitsSliceSegmentAddress);
	}
	else; 	//original slice segment address = 0

	if (new_address != 0) //new address != 0  						    
	{
		if (pps->dependent_slice_segments_enabled_flag)				    //dependent_slice_segment_flag WRITE
		{
			gf_bs_write_int(bs_rw, 0, 1);
		}						    
		gf_bs_write_int(bs_rw, new_address, sps->bitsSliceSegmentAddress);	    //slice_segment_address WRITE
	}
	else //new_address = 0
	{
		//gf_bs_write_int(bs_rw, new_address, 1);
	}

	/*PicSizeInCtbsY = PicWidthInCtbsY * PicHeightInCtbsY;
	gf_bs_read_int(bs_ori, ceil(Log2( PicSizeInCtbsY))	*/
	
	//******
	
	//testing
	/*int slice_reserved_flag = gf_bs_read_int(bs_ori, pps->num_extra_slice_header_bits);
	int slice_type = bs_get_ue(bs_ori);
	int pic_output_flag = 999;
	int colour_plane_id = 999;
	if(pps->output_flag_present_flag)
		pic_output_flag = gf_bs_read_int(bs_ori, 1);
	if (sps->separate_colour_plane_flag == 1)
		colour_plane_id = gf_bs_read_int(bs_ori, 2);
	printf("===================\n");
	printf("new address: %d\n", new_address);
	printf("slice_reserved_flag: %d\n", slice_reserved_flag);
	printf("slice_type: %d %d\n", slice_type);
	printf("pic_output_flag: %d\n", pic_output_flag);
	printf("colour_plane_id: %d\n", colour_plane_id);
	printf("slice_pic_order_cnt_lsb: %d\n", hevc->s_info.poc_lsb);
	printf("===================\n");*/
	//

	while (gf_bs_get_size(bs_ori)*8 != gf_bs_get_position(bs_ori)*8+gf_bs_get_bit_position(bs_ori)) //Rest contents copying
	{
		u32 rest_contents = gf_bs_read_int(bs_ori, 1);
		gf_bs_write_int(bs_rw, rest_contents, 1);
	}
	

	gf_bs_align(bs_rw);						//align

	*out_slice_length = 0;
	*out_slice = NULL;
	gf_bs_get_content(bs_rw, out_slice, out_slice_length);

//http://what-when-how.com/Tutorial/topic-397pct9eq3/High-Efficiency-Video-Coding-HEVC-44.html
} 

void parse_and_print_VPS(char *buffer, u32 nal_length, HEVCState* hevc)
{
	int i = gf_media_hevc_read_vps(buffer, nal_length, hevc);
	printf("vps id:\t%d\n", i);
	printf("num_profile_tier_level:\t%d\n",(*hevc).vps[i].num_profile_tier_level);
}


void parse_and_print_SPS(char *buffer, u32 nal_length, HEVCState* hevc)
{	
	int i = gf_media_hevc_read_sps(buffer, nal_length, hevc);
	printf("sps id:\t%d\n", i);
       	printf("width:\t%d\n",(*hevc).sps[i].width);
	printf("height:\t%d\n",(*hevc).sps[i].height);
	printf("cw_flag:\t%d\n",(*hevc).sps[i].cw_flag);
	printf("max_CU_width:\t%d\n",(*hevc).sps[i].max_CU_width);
	printf("max_CU_height:\t%d\n",(*hevc).sps[i].max_CU_height);
	printf("sar_width:\t%d\n",(*hevc).sps[i].sar_width);
	printf("sar_height:\t%d\n",(*hevc).sps[i].sar_height);
}

void parse_and_print_PPS(char *buffer, u32 nal_length, HEVCState* hevc)
{	
	int i = gf_media_hevc_read_pps(buffer, nal_length, hevc);
 	printf("pps id:\t%d\n", i);
    	printf("column_width:\t%d\n",(*hevc).pps[i].column_width[i]);
     	printf("uniform_spacing_flag:\t%d\n",(*hevc).pps[i].uniform_spacing_flag);
     	printf("dependent_slice_segments_enabled_flag:\t%d\n",(*hevc).pps[i].dependent_slice_segments_enabled_flag);
	printf("num_tile_columns:\t%d\n",(*hevc).pps[i].num_tile_columns);
	printf("num_tile_rows:\t%d\n",(*hevc).pps[i].num_tile_rows);
	printf("num_profile_tier_level:\t%d\n",(*hevc).vps[i].num_profile_tier_level);
	printf("height:\t%d\n",(*hevc).sps[i].height);
		
}

/*void printSliceInfo(HEVCState hevc){
	printf("===SSH===\n");
	int i;
	for(i=0;i<2;i++){
  		printf("id:	%d\n",hevc.pps[i].id);
		printf("tiles_enabled_flag:	%d\n",hevc.pps[i].tiles_enabled_flag);
	     	printf("uniform_spacing_flag:	%d\n",hevc.pps[i].uniform_spacing_flag);
		printf("dependent_slice_segments_enabled_flag:	%d\n",hevc.pps[i].dependent_slice_segments_enabled_flag);
		printf("num_tile_columns:	%d\n",hevc.pps[i].num_tile_columns);
		printf("num_tile_rows:	%d\n",hevc.pps[i].num_tile_rows);
		}
}*/


u64 size_of_file(FILE *file)
{
	long cur = ftell(file);
	fseek(file, 0L, SEEK_END);
	u64 size = (u64) ftell(file);
	fseek(file, cur, SEEK_SET);
	return size;
}


int main (int argc, char **argv)
{
	printf("===begin====\n");
	FILE* file = NULL;
	FILE* file_output = NULL;
	int tile_1 = atoi(argv[2])-1;
	int tile_2 = atoi(argv[3])-1;
	if(4 == argc){
		file = fopen(argv[1], "rb");
		file_output = fopen("/home/ubuntu/gpac/applications/tiles/output_film.hvc", "wb");
                if(file != NULL){
			//print_nalu(file);
			u64 length_no_use = 0;
			GF_BitStream *bs = gf_bs_from_file(file, GF_BITSTREAM_READ);; //==The whole bitstream
			GF_BitStream *bs_swap = gf_bs_from_file(file_output, GF_BITSTREAM_WRITE);
			char *buffer = NULL; //==pointer to hold chunk of NAL data
			char *buffer_swap;
			u32 nal_start_code;
			int nal_start_code_length = 0;
			u32 nal_length;
			u32 nal_length_swap;
			int is_nal = 0;
			int num_vps = 1;
			int num_sps = 1;
			int num_pps = 1;
			int tiles_num = 0;
			int slice_address[100] = {0};
			int slice_count = 0;

			//Reading the slice addresses
			if (bs != NULL)
			{
				HEVCState hevc;
				u8 nal_unit_type =1, temporal_id, layer_id;
				while(gf_bs_get_size(bs)!= gf_bs_get_position(bs))
				{
					nal_length = gf_media_nalu_next_start_code_bs(bs);
					gf_bs_skip_bytes(bs, nal_length);
					nal_start_code = gf_bs_read_int(bs, 24);
					if(0x000001 == nal_start_code)
					{
						is_nal = 1;
					}
					else
					{
						if(0x000000 == nal_start_code && 1==gf_bs_read_int(bs, 8))
						{
							is_nal = 1;
						}
						
						else
						{
							is_nal = 0;
						}
					}
					if(is_nal)
					{
						nal_length = gf_media_nalu_next_start_code_bs(bs);
						if(nal_length == 0)
						{
							nal_length = gf_bs_get_size(bs) - gf_bs_get_position(bs);
						}
						buffer = malloc(sizeof(char)*nal_length);
						gf_bs_read_data(bs,buffer,nal_length);
						GF_BitStream *bs_tmp = gf_bs_new(buffer, nal_length+1, GF_BITSTREAM_READ);
						gf_media_hevc_parse_nalu(bs_tmp, &hevc, &nal_unit_type, &temporal_id, &layer_id);
						if (nal_unit_type == 33)						
						{
							gf_media_hevc_read_sps(buffer, nal_length, &hevc);
						}
						else if (nal_unit_type == 34)						
						{
							int i = gf_media_hevc_read_pps(buffer, nal_length, &hevc);
							tiles_num = hevc.pps[i].num_tile_columns * hevc.pps[i].num_tile_rows;
						}
						else if (nal_unit_type <= 21)
						{
							if (slice_count != tiles_num)
							{
								slice_address[slice_count] = hevc.s_info.slice_segment_address;
								slice_count++;
							}
							else;							
						}
						else;
					}
				}
				gf_bs_seek(bs, 0);
				free(buffer);
			}
			printf("Tiles num: %d\n", tiles_num);
			printf("Slice address:");
			int i = 0;			
			for (i; i<slice_count; i++)
			{
				printf(" %d", slice_address[i]);
			}
			printf("\n");
			char *buffer_reorder[slice_count];
			u32 buffer_reorder_length[slice_count];
			u32 buffer_reorder_sc[slice_count][2];

			//Start parsing and swapping
			if (bs != NULL)
			{
				HEVCState hevc;
				u8 nal_unit_type =1, temporal_id, layer_id;
				int nal_num=0;
				int slice_num=0;
				int first_slice_num=0;
				
				while(gf_bs_get_size(bs)!= gf_bs_get_position(bs))
				{
					nal_length = gf_media_nalu_next_start_code_bs(bs);
					gf_bs_skip_bytes(bs, nal_length);
					nal_start_code = gf_bs_read_int(bs, 24);
					if(0x000001 == nal_start_code)
					{
						is_nal = 1;
						nal_start_code_length = 24;
					}
					else
					{
						if(0x000000 == nal_start_code && 1==gf_bs_read_int(bs, 8))
						{
							is_nal = 1;
							nal_start_code = 0x00000001;
							nal_start_code_length = 32;
						}
						
						else
						{
							is_nal = 0;
						}
					}

					if(is_nal)
					{
						nal_length = gf_media_nalu_next_start_code_bs(bs);
						//printf("\n\n");
						if(nal_length == 0)
						{
							nal_length = gf_bs_get_size(bs) - gf_bs_get_position(bs);
						}
						buffer = malloc(sizeof(char)*nal_length);
						gf_bs_read_data(bs,buffer,nal_length);
						GF_BitStream *bs_tmp = gf_bs_new(buffer, nal_length+1, GF_BITSTREAM_READ);
						gf_media_hevc_parse_nalu(bs_tmp, &hevc, &nal_unit_type, &temporal_id, &layer_id);
						nal_num++;	
						//printf("nal_number: \t%d\t%d\n ",nal_num,nal_unit_type);
						switch (nal_unit_type)
						{
							case 32:
								printf("===VPS #%d===\n", num_vps);
								parse_and_print_VPS(buffer, nal_length, &hevc);
								gf_bs_write_int(bs_swap, nal_start_code, nal_start_code_length);
								gf_bs_write_data(bs_swap, buffer, nal_length);
								num_vps++;
								break;
							case 33:
								printf("===SPS #%d===\n", num_sps);
								parse_and_print_SPS(buffer, nal_length, &hevc);
								gf_bs_write_int(bs_swap, nal_start_code, nal_start_code_length);
								gf_bs_write_data(bs_swap, buffer, nal_length);
								num_sps++;
								break;
							case 34:
								printf("===PPS #%d===\n", num_pps);
								parse_and_print_PPS(buffer, nal_length, &hevc);
								gf_bs_write_int(bs_swap, nal_start_code, nal_start_code_length);
								gf_bs_write_data(bs_swap, buffer, nal_length);
								num_pps++;
								break;
							default:
								if (nal_unit_type <= 21)  //Slice
								{
									//printf("===Slice===\n");
									//printf("Slice adresse\t:%d\n",hevc.s_info.slice_segment_address);
									//printf("first_slice_segment_in_pic_flag\t:%d\n",hevc.s_info.first_slice_segment_in_pic_flag);
									//printf("frame_num\t:%d\n",hevc.s_info.frame_num);
									if (hevc.s_info.slice_segment_address == slice_address[tile_1])
									{
										buffer_reorder_sc[tile_2][0] = nal_start_code;
										buffer_reorder_sc[tile_2][1] = nal_start_code_length;
										rewrite_slice_address(slice_address[tile_2], buffer, nal_length, &buffer_swap, &nal_length_swap, &hevc);
										if(nal_length_swap!=nal_length) fprintf(stderr, "ERROR in size\n");
										buffer_reorder_length[tile_2] = sizeof(char)*nal_length_swap;										
										buffer_reorder[tile_2] = malloc(sizeof(char)*nal_length_swap);										
										memcpy(buffer_reorder[tile_2], buffer_swap, sizeof(char)*nal_length_swap);
										//printf("I/O nal length: %d %d\n", nal_length, nal_length_swap);
										free(buffer_swap);
									}	
									else if (hevc.s_info.slice_segment_address == slice_address[tile_2])
									{			
										buffer_reorder_sc[tile_1][0] = nal_start_code;
										buffer_reorder_sc[tile_1][1] = nal_start_code_length;		
										rewrite_slice_address(slice_address[tile_1], buffer, nal_length, &buffer_swap, &nal_length_swap, &hevc);
										if(nal_length_swap!=nal_length) fprintf(stderr, "ERROR in size\n");
										buffer_reorder_length[tile_1] = sizeof(char)*nal_length_swap;										
										buffer_reorder[tile_1] = malloc(sizeof(char)*nal_length_swap);										
										memcpy(buffer_reorder[tile_1], buffer_swap, sizeof(char)*nal_length_swap);
										free(buffer_swap);							
									}
									else //no swap
									{
										int slice_num;
										if (hevc.s_info.slice_segment_address == slice_address[0])
											slice_num = 0;
										else if (hevc.s_info.slice_segment_address == slice_address[1])
											slice_num = 1;
										else if (hevc.s_info.slice_segment_address == slice_address[2])
											slice_num = 2;
										else if (hevc.s_info.slice_segment_address == slice_address[3])
											slice_num = 3;
										else if (hevc.s_info.slice_segment_address == slice_address[4])
											slice_num = 4;
										else if (hevc.s_info.slice_segment_address == slice_address[5])
											slice_num = 5;
										else if (hevc.s_info.slice_segment_address == slice_address[6])
											slice_num = 6;
										else if (hevc.s_info.slice_segment_address == slice_address[7])
											slice_num = 7;
										else if (hevc.s_info.slice_segment_address == slice_address[8])
											slice_num = 8;
										else
											printf("slice address doesn't match!\n");
										buffer_reorder_sc[slice_num][0] = nal_start_code;
										buffer_reorder_sc[slice_num][1] = nal_start_code_length;
										buffer_reorder_length[slice_num] = sizeof(char)*nal_length;
										buffer_reorder[slice_num] = malloc(sizeof(char)*nal_length);										
										memcpy(buffer_reorder[slice_num], buffer, sizeof(char)*nal_length);
									}

									if (hevc.s_info.slice_segment_address == slice_address[8])  //re-ordering
									{
										int i=0;
										for (i; i<9; i++)
										{
											gf_bs_write_int(bs_swap, buffer_reorder_sc[i][0], buffer_reorder_sc[i][1]);
											gf_bs_write_data(bs_swap, buffer_reorder[i], buffer_reorder_length[i]);
										}
										
										int j=0;										
										for (j; j<9; j++)
										{
											free(buffer_reorder[j]);
										}
									}
									else;

									if (hevc.s_info.first_slice_segment_in_pic_flag)
										first_slice_num++;
									else;
									slice_num++;
									//gf_bs_write_data(bs_swap, buffer, nal_length);
									//printf("input pos: %d\n", gf_bs_get_position(bs)*8+gf_bs_get_bit_position(bs));
									//printf("output pos: %d\n", gf_bs_get_position(bs_swap)*8+gf_bs_get_bit_position(bs_swap));	
								} 
								else 		//Not slice
								{
									gf_bs_write_int(bs_swap, nal_start_code, nal_start_code_length);
									gf_bs_write_data(bs_swap, buffer, nal_length);
								}
								break;
							//default:
								//printf("nal_unit_type not managed\n\n");
								//gf_bs_write_data(bs_swap, buffer, nal_length);
								//break;

						}
						free(buffer);
						//free(*buffer_swap);
						
					}
					else{
						//printf("6\n");
						//printf("==%lu==\n",(gf_bs_get_size(bs) - gf_bs_get_position(bs)));
						//printf("s==%lu==\n",gf_bs_get_size(bs));
						//printf("p==%lu==\n", gf_bs_get_position(bs));
						//gf_bs_skip_bytes(bs, gf_bs_get_size(bs) - gf_bs_get_position(bs));	
					}
				}
				printf("======Log======\n");
				printf("input bs size: %llu\n", gf_bs_get_size(bs));
				printf("output bs size: %llu\n", gf_bs_get_size(bs_swap));
				printf("slice number: %d\n", slice_num); 
				printf("first slice number: %d\n", first_slice_num); 
				printf("slice number/frame: %d\n", slice_num/first_slice_num); 

				fclose(file_output);
			}

                }
	}
	return 0;	
}

