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
#include <math.h>
#include "helper.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>


// Extract the tiles from a tiled video
void extract_tiles (int argc, char **argv)
{
    FILE* file = NULL;
    int i,j;
    if(3 <= argc)
    {
        file = fopen(argv[2],"rb");
        if(file != NULL)
        {
            GF_BitStream *bs;
            bs = gf_bs_from_file(file, GF_BITSTREAM_READ);
            if(bs != NULL)
            {
                //Store the first before start code in the bitstream
                char *buffer_swap=NULL, *SEI_buffer=NULL; //buffer for swap operation
                char *buffer =NULL; //buffer to store one NAL data
                char *VPS_buffer=NULL; //Store the VPS
                char *SPS_buffer=NULL; //Store the SPS
                char *PPS_buffer=NULL; //Store the PPS
                u32 buffer_swap_length=0, buffer_length=0, SEI_buffer_length, VPS_length=0, SPS_length=0, PPS_length=0; 
                u32 nal_start_code;
                int is_nal;
                u32 x, y, pps_id;
		HEVCState hevc;
		u8 nal_unit_type, temporal_id, layer_id;
                Bool is_SEI=0;
                
                /**array of bitstream: Each one point to one tile bistream*/
		GF_BitStream *bs_tiles[10][10]; 
                /**array of file: Each one point to one file where one tile bistream will be saved*/
                FILE *files[10][10];
		while( gf_bs_get_size(bs)!= gf_bs_get_position(bs))
		{
                    buffer_length = gf_media_nalu_next_start_code_bs(bs);
                    gf_bs_skip_bytes(bs, buffer_length);
                    nal_start_code = gf_bs_read_int(bs, 24);
                    if(0x000001 == nal_start_code){
                        is_nal = 1;
                    }
                    else{
                        if(0x000000 == nal_start_code && 1==gf_bs_read_int(bs, 8)){
                            is_nal = 1;
                        }

                        else{
                            is_nal = 0;
                        }
                    }

                    if(is_nal)
                    {
			buffer_length = gf_media_nalu_next_start_code_bs(bs);
                        if(0==buffer_length)
                            buffer_length = gf_bs_get_size(bs) - gf_bs_get_position(bs);

			buffer = malloc(sizeof(char)*buffer_length);
			gf_bs_read_data(bs,buffer,buffer_length);
			GF_BitStream *bs_buffer = gf_bs_new(buffer, buffer_length, GF_BITSTREAM_READ);
			gf_media_hevc_parse_nalu(bs_buffer, &hevc, &nal_unit_type, &temporal_id, &layer_id);
                        gf_bs_del(bs_buffer);
                        switch(nal_unit_type){
                            case 32:
                                VPS_buffer = buffer;
                                VPS_length = buffer_length;
                                gf_media_hevc_read_vps(VPS_buffer,VPS_length, &hevc);
                                buffer = NULL;
                                break;
                            case 33:
                                SPS_buffer = buffer;
                                SPS_length = buffer_length;
                                gf_media_hevc_read_sps(SPS_buffer,SPS_length, &hevc);
                                buffer = NULL;
                                break;
                            case 34:
                                PPS_buffer = buffer;
                                PPS_length = buffer_length;
                                pps_id = gf_media_hevc_read_pps(PPS_buffer,PPS_length, &hevc);
                                buffer = NULL;
                                break;
                            case 21:
                            case 19:
                            case 18:
                            case 17:
                            case 16:
                            case 9:
                            case 8:
                            case 7:
                            case 6:
                            case 5:
                            case 4:
                            case 2:
                            case 0:
                            case 1:
                                if(hevc.s_info.pps->tiles_enabled_flag)
                                {
                                    get_2D_cordinates_of_slices(&x, &y, &hevc);
                                    printf("(x,y):\t(%u,%u)",x,y);
                                    if(bs_tiles[x][y]== NULL) // Check if it is the first slice of tile
                                    {
                                        char tile_name[100];
                                        /** argv[3] is the directory where the tiles will be saved*/
                                        sprintf(tile_name, "%s/tile_%u_%u_.hvc",argv[3], x, y);
                                        files[x][y] = fopen(tile_name,"wb");
                                        //create bitstream
                                        bs_tiles[x][y] = gf_bs_from_file(files[x][y],GF_BITSTREAM_WRITE);
                                        //Copy the VPS
                                        gf_bs_write_int(bs_tiles[x][y], 1, 32);
                                        gf_bs_write_data(bs_tiles[x][y],VPS_buffer, VPS_length);
                                        gf_bs_write_int(bs_tiles[x][y], 1, 32);
                                        
                                        //rewrite and copy the SPS
                                        rewrite_SPS(SPS_buffer, SPS_length, &buffer_swap, &buffer_swap_length, &hevc);
                                        printf("\nSPS (in, out)=(%u,%u)\n", SPS_length,buffer_swap_length);
                                        gf_bs_write_data(bs_tiles[x][y],buffer_swap, buffer_swap_length);
                                        free(buffer_swap);
                                        buffer_swap = NULL;
                                        buffer_swap_length = 0;
                                        
                                        //rewrite and copy the PPS
                                        gf_bs_write_int(bs_tiles[x][y], 1, 32);
                                        rewrite_PPS(PPS_buffer,PPS_length, &buffer_swap, &buffer_swap_length);
                                        gf_bs_write_data(bs_tiles[x][y],buffer_swap, buffer_swap_length);
                                        free(buffer_swap);
                                        buffer_swap = NULL;
                                        //l += (start_length+VPS_length+SPS_length+buffer_swap_length+12);
                                        buffer_swap_length = 0;
                                        
                                        if(is_SEI)
                                        {
                                            gf_bs_write_int(bs_tiles[x][y], 1, 32);
                                            gf_bs_write_data(bs_tiles[x][y],SEI_buffer, SEI_buffer_length);
                                        }
                                        
                                        //gf_bs_write_data(bs_tiles[x][y],PPS_buffer, PPS_length);
                                    }
                                    
                                    //rewrite and copy the Slice
                                    gf_bs_write_int(bs_tiles[x][y], 1, 32);
                                    rewrite_slice_address(0, buffer, buffer_length, &buffer_swap, &buffer_swap_length, &hevc);
                                    gf_bs_write_data(bs_tiles[x][y], buffer_swap, buffer_swap_length);
                                    free(buffer_swap);
                                    buffer_swap = NULL;
                                    buffer_swap_length = 0;
                                    
                                    free(buffer);
                                    buffer = NULL;
                                    buffer_length = 0;
                               }
                                else{
                                    printf("\n Make sure you video is tiled. \n");
                                }
                                
                                break;
                            default:
                                for(i=0;i<hevc.pps[pps_id].num_tile_columns;i++)
                                {
                                    for(j=0;j<hevc.pps[pps_id].num_tile_rows;j++)
                                    {
                                        if(bs_tiles[i][j])
                                        {
                                            gf_bs_write_int(bs_tiles[i][j], 1, 32);
                                            gf_bs_write_data(bs_tiles[i][j], buffer, buffer_length);
                                        }
                                        else{
                                            is_SEI = 1;
                                        }
                                    }
                                }
                                if(is_SEI)
                                {
                                    SEI_buffer = buffer;
                                    SEI_buffer_length = buffer_length;
                                }
                                buffer = NULL;
                                buffer_length = 0;
                                break;
			}
                    }else{
                        printf("\n Your file is corrupted. \n");
                        goto exit;
                    }
                }
                for(i=0;i<hevc.s_info.pps->num_tile_columns;i++)
                {
                    for(j=0;j<hevc.s_info.pps->num_tile_rows;j++)
                    {
                        gf_bs_del(bs_tiles[i][j]);
                        fclose(files[i][j]);
                    }
                }		
            }else{
                printf("\n Sorry try again: our program failed to open the bitsream. \n");
                goto exit;
            }
            //fclose(file);
        }else
        {
            printf("\n Make sure you file path is correct. \n");
            goto exit;
        }
        
    }
    exit:
        fclose(file);           
}

void swap_tiles (int argc, char **argv)
{
	printf("===begin====\n");
	FILE* file = NULL;
	FILE* file_output = NULL;
	int tile_1 = atoi(argv[3])-1;
	int tile_2 = atoi(argv[4])-1;
	if(6 == argc)
	{
		file = fopen(argv[2], "rb");
		file_output = fopen(argv[5], "wb");
                if(file != NULL)
		{
			GF_BitStream *bs = gf_bs_from_file(file, GF_BITSTREAM_READ);; //==The whole bitstream
			
			if (bs != NULL)
			{
				GF_BitStream *bs_swap = gf_bs_from_file(file_output, GF_BITSTREAM_WRITE);
				HEVCState hevc;
				u8 nal_unit_type, temporal_id, layer_id;
				int nal_num=0;
				int slice_num=0;
				int first_slice_num=0;
				int slice_info_scan_finish = 0;
				char *buffer_reorder[100];
				u32 buffer_reorder_length[100];
				u32 buffer_reorder_sc[100][2];
				char *buffer = NULL; //==pointer to hold chunk of NAL data
				char *buffer_swap;
				u32 nal_start_code;
				int nal_start_code_length;
				u32 nal_length;
				u32 nal_length_swap;
				int is_nal = 0;
				int vps_num = 0;
				int sps_num = 0;
				int pps_num = 0;
				int tile_num = 0;
				int slice_address[100] = {0};
				int pos_rec = 0;
				int tile_info_check = 0;
				int tiles_width[100] = {0};
				int tiles_height[100] = {0};
				u32 tile_x, tile_y, tile_width, tile_height;
				
				while(gf_bs_get_size(bs) != gf_bs_get_position(bs) && tile_info_check == 0)
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
						if(nal_length == 0)
						{
							nal_length = gf_bs_get_size(bs) - gf_bs_get_position(bs);
						}
						buffer = malloc(sizeof(char)*nal_length);
						gf_bs_read_data(bs,buffer,nal_length);
						GF_BitStream *bs_tmp = gf_bs_new(buffer, nal_length, GF_BITSTREAM_READ);
						//printf("size bs buf bstmp:%d %lld %lld\n",sizeof(char)*nal_length, gf_bs_get_position(bs), gf_bs_get_size(bs_tmp));
						gf_media_hevc_parse_nalu(bs_tmp, &hevc, &nal_unit_type, &temporal_id, &layer_id);
						nal_num++;
						//printf("%d ", nal_unit_type);	
						switch (nal_unit_type)
						{
							case 32:
								vps_num++;
								printf("===VPS #%d===\n", vps_num);
								parse_print_VPS(buffer, nal_length, &hevc);
								gf_bs_write_int(bs_swap, nal_start_code, nal_start_code_length);
								gf_bs_write_data(bs_swap, buffer, nal_length);
								if (pos_rec < gf_bs_get_position(bs))
									pos_rec = gf_bs_get_position(bs);
								break;
							case 33:
								sps_num++;
								printf("===SPS #%d===\n", sps_num);
								parse_print_SPS(buffer, nal_length, &hevc);
								gf_bs_write_int(bs_swap, nal_start_code, nal_start_code_length);
								gf_bs_write_data(bs_swap, buffer, nal_length);
								if (pos_rec < gf_bs_get_position(bs))
									pos_rec = gf_bs_get_position(bs);
								break;
							case 34:
								pps_num++;
								printf("===PPS #%d===\n", pps_num);
								parse_and_print_PPS(buffer, nal_length, &hevc, &tile_num);
								gf_bs_write_int(bs_swap, nal_start_code, nal_start_code_length);
								gf_bs_write_data(bs_swap, buffer, nal_length);
								if (tile_1+1 > tile_num || tile_2+1 > tile_num)
								{
									tile_info_check = 1;
								}
								if (pos_rec < gf_bs_get_position(bs))
									pos_rec = gf_bs_get_position(bs);
								break;
							default:
								if (nal_unit_type <= 21)  //Slice
								{
									//printf("===Slice===\n");
									//printf("Slice adresse\t:%d\n",hevc.s_info.slice_segment_address);
									//printf("first_slice_segment_in_pic_flag\t:%d\n",hevc.s_info.first_slice_segment_in_pic_flag);
									//printf("frame_num\t:%d\n",hevc.s_info.frame_num);

									int tile_id = hevc_get_tile_id(&hevc, &tile_x, &tile_y, &tile_width, &tile_height);
									//printf("x,y,w,h: %d %d %d %d\n", tile_x, tile_y, tile_width, tile_height);
									tiles_width[tile_id] = tile_width;
									tiles_height[tile_id] = tile_height;

									if (!slice_info_scan_finish)
									{
										slice_address_calculation(&hevc, &slice_address[tile_id], tile_x, tile_y);
										if (slice_address[tile_id] != hevc.s_info.slice_segment_address)
											printf("Slice address calculation wrong!\n");
										if (tile_id == tile_num-1)
										{
											slice_info_scan_finish = 1;	
											gf_bs_seek(bs, pos_rec);
											if (tiles_width[tile_1] != tiles_width[tile_2] || tiles_height[tile_1] != tiles_height[tile_2])
											{
												tile_info_check = 2;
											}		
										}
									}
									else
									{												
										if (slice_address[tile_id] == slice_address[tile_1])
										{
											buffer_reorder_sc[tile_2][0] = nal_start_code;
											buffer_reorder_sc[tile_2][1] = nal_start_code_length;
											rewrite_slice_address(slice_address[tile_2], buffer, nal_length, &buffer_swap, &nal_length_swap, &hevc);
											//printf("output bs size2: %d\n", nal_length_swap);
											//if(nal_length_swap!=nal_length) fprintf(stderr, "ERROR in size\n");
											buffer_reorder_length[tile_2] = sizeof(char)*nal_length_swap;										
											buffer_reorder[tile_2] = malloc(sizeof(char)*nal_length_swap);										
											memcpy(buffer_reorder[tile_2], buffer_swap, sizeof(char)*nal_length_swap);
											free(buffer_swap);
										}	
										else if (slice_address[tile_id] == slice_address[tile_2])
										{			
											buffer_reorder_sc[tile_1][0] = nal_start_code;
											buffer_reorder_sc[tile_1][1] = nal_start_code_length;		
											rewrite_slice_address(slice_address[tile_1], buffer, nal_length, &buffer_swap, &nal_length_swap, &hevc);
											//printf("output bs size2: %d\n", nal_length_swap);
											//if(nal_length_swap!=nal_length) fprintf(stderr, "ERROR in size\n");
											buffer_reorder_length[tile_1] = sizeof(char)*nal_length_swap;										
											buffer_reorder[tile_1] = malloc(sizeof(char)*nal_length_swap);										
											memcpy(buffer_reorder[tile_1], buffer_swap, sizeof(char)*nal_length_swap);
											free(buffer_swap);							
										}
										else //no swap
										{
											int slice_num;
											int i=0;
											for (i; i<tile_num; i++)
											{
												if (slice_address[tile_id] == slice_address[i])
													slice_num = i;
												else;
											}
											buffer_reorder_sc[slice_num][0] = nal_start_code;
											buffer_reorder_sc[slice_num][1] = nal_start_code_length;
											buffer_reorder_length[slice_num] = sizeof(char)*nal_length;
											buffer_reorder[slice_num] = malloc(sizeof(char)*nal_length);										
											memcpy(buffer_reorder[slice_num], buffer, sizeof(char)*nal_length);
										}

										if (tile_id == tile_num-1)  //re-ordering
										{
											int i=0;
											for (i; i<tile_num; i++)
											{
												gf_bs_write_int(bs_swap, buffer_reorder_sc[i][0], buffer_reorder_sc[i][1]);
												gf_bs_write_data(bs_swap, buffer_reorder[i], buffer_reorder_length[i]);
											}
										
											int j=0;										
											for (j; j<tile_num; j++)
											{
												free(buffer_reorder[j]);
											}
										}
										else;

										if (hevc.s_info.first_slice_segment_in_pic_flag)
											first_slice_num++;
										else;
										slice_num++;
										//printf("input pos: %d\n", gf_bs_get_position(bs)*8+gf_bs_get_bit_position(bs));
										//printf("output pos: %d\n", gf_bs_get_position(bs_swap)*8+gf_bs_get_bit_position(bs_swap));	
									}								
								} 
								else 		//Not slice
								{
									gf_bs_write_int(bs_swap, nal_start_code, nal_start_code_length);
									gf_bs_write_data(bs_swap, buffer, nal_length);
									if (pos_rec < gf_bs_get_position(bs) && !slice_info_scan_finish)
										pos_rec = gf_bs_get_position(bs);
								}
								break;
						}
						free(buffer);
						//free(*buffer_swap);
						
					}
					else;
				}
				printf("======Log======\n");
				if (tile_info_check == 0)
				{
					printf("Tile num: %d\n", tile_num);
					printf("Slice address:");
					int i = 0;			
					for (i; i<tile_num; i++)
					{
						printf(" %d", slice_address[i]);
					}
					printf("\n");
					printf("input bs size: %lu\n", gf_bs_get_size(bs));
					printf("output bs size: %lu\n", gf_bs_get_size(bs_swap));
					printf("slice number: %d\n", slice_num); 
					printf("first slice number: %d\n", first_slice_num); 
					printf("slice number/frame: %d\n", slice_num/first_slice_num);
					printf("Success!\n"); 
				}
				else if (tile_info_check == 1)
				{
					printf("Requested tile id doesn't exist!\n");
					if (tile_num == 0)
						printf("This file has no tile!\n");
					else
						printf("This file has only %d tile(s)\n", tile_num);
				}
				else if (tile_info_check == 2)
				{
					printf("Tile sizes are different, can't swap!\n");
					printf("Tile 1: %dx%d, Tile 2: %dx%d\n", tiles_width[tile_1], tiles_height[tile_1], tiles_width[tile_2], tiles_height[tile_2]);
				}
				else;

				fclose(file_output);
			}
			else
				printf("Bitstream reading fail!\n");
                }
		else
			printf("File reading fail!\n");
	}
	return 0;	
}

void bs_info(int argc, char **argv)
{
    FILE* file = NULL;
    if(3 <= argc)
    {
        file = fopen(argv[2],"rb");
        if(file != NULL)
        {
            GF_BitStream *bs;
            bs = gf_bs_from_file(file, GF_BITSTREAM_READ);
            if(bs != NULL)
            {
                char *buffer =NULL;
                u32 buffer_length; 
                u32 nal_start_code;
                int is_nal;
                u32 x, y;
		HEVCState hevc;
		u8 nal_unit_type, temporal_id, layer_id;
		int nal_num=0;
		while( gf_bs_get_size(bs)!= gf_bs_get_position(bs))
		{
                    buffer_length = gf_media_nalu_next_start_code_bs(bs);
                    gf_bs_skip_bytes(bs, buffer_length);
                    nal_start_code = gf_bs_read_int(bs, 24);
                    if(0x000001 == nal_start_code){
                        is_nal = 1;
                    }
                    else{
                        if(0x000000 == nal_start_code && 1==gf_bs_read_int(bs, 8)){
                            is_nal = 1;
                        }

                        else{
                            is_nal = 0;
                        }
                    }

                    if(is_nal)
                    {
			buffer_length = gf_media_nalu_next_start_code_bs(bs);
                        if(0==buffer_length)
                            buffer_length = gf_bs_get_size(bs) - gf_bs_get_position(bs);

			buffer = malloc(sizeof(char)*buffer_length);
			gf_bs_read_data(bs,buffer,buffer_length);
			GF_BitStream *bs_buffer = gf_bs_new(buffer, buffer_length, GF_BITSTREAM_READ);
			gf_media_hevc_parse_nalu(bs_buffer, &hevc, &nal_unit_type, &temporal_id, &layer_id);
                        gf_bs_del(bs_buffer);
			nal_num++;
                        switch(nal_unit_type){
                            case 32:
                                parse_print_VPS(buffer,buffer_length, &hevc);
                                break;
                            case 33:
                                parse_print_SPS(buffer,buffer_length, &hevc);
                                break;
                            case 34:
                                parse_print_PPS(buffer,buffer_length, &hevc);
                                break;
                            case 21:
                            case 19:
                            case 18:
                            case 17:
                            case 16:
                            case 9:
                            case 8:
                            case 7:
                            case 6:
                            case 5:
                            case 4:
                            case 2:
                            case 0:
                            case 1:
                                get_size_of_slice(&x, &y, &hevc);
                                printf("\n slice: (width,height) = (%u,%u) \t adress: %u", x, y, hevc.s_info.slice_segment_address);
                                get_2D_cordinates_of_slices(&x, &y, &hevc);printf("\n slice: (wx,hy) = (%u,%u) \t adress: %u", x, y, hevc.s_info.slice_segment_address);
                                break;
                            default:
                                break;
			}
                    }
                    else{
                        printf("\n Your file is corrupted. \n");
                        goto exit;
                    }
                }
                printf("\n\nnumber of NAL:\t %d",nal_num);
            }else{
                printf("\n Sorry try again: our program failed to open the bitsream. \n");
                goto exit;
            }
            //fclose(file);
        }else
        {
            printf("\n Make sure you file path is correct. \n");
            goto exit;
        }
    }
    exit:
        fclose(file);                        
}

void test()
{
    char data[12];
    char *dest;
    sprintf(data,"tester");
    GF_BitStream *bs_in = gf_bs_new(data, 6, GF_BITSTREAM_READ);
    GF_BitStream *bs_out = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
    printf("size:  %lu\n\n",gf_bs_get_size(bs_in));
    printf("r_bit_pos:  %u\t",gf_bs_get_bit_position(bs_in));
    printf("w_bit_pos:  %u\t",gf_bs_get_bit_position(bs_out));
    printf("r_byte_pos:  %lu\t",gf_bs_get_position(bs_in));
    printf("w_byte_pos:  %lu\t",gf_bs_get_position(bs_out));
    gf_bs_write_int(bs_out,gf_bs_read_int(bs_in,1),1);
    int i= 0;
    for(i=0;i<20;i++){
        printf("\n");
        printf("r_bit_pos:  %u\t",gf_bs_get_bit_position(bs_in));
        printf("w_bit_pos:  %u\t",gf_bs_get_bit_position(bs_out));
        printf("r_byte_pos:  %lu\t",gf_bs_get_position(bs_in));
        printf("w_byte_pos:  %lu\t",gf_bs_get_position(bs_out));
        gf_bs_write_int(bs_out,gf_bs_read_int(bs_in,1),1);
    }
}


int combine_tiles(int argc, char *argv[])
{   int combination_enabled_flag=1;
    int num_video=argc-2;
    FILE* video[argc];
    FILE* file_output = NULL;
    int i,j,k;

    for(i=2; i<argc; i++)//read the videos
       {
            video[i] = fopen(argv[i], "r");
       }
      printf("Number of videos :%d\n",num_video);
  file_output = fopen("/home/ubuntu/gpac/applications/tiles/output_combination.hvc", "wb");
  int max_x=0,max_y=0,num_tile_rows_minus1=0,num_tile_columns_minus1=0;
  int position[argc][2];//specify the position of each video
      for(j=1;j<=num_video;j++)
         { 
             printf("Position of video %d (x,y):\n",j);
             scanf("%d%d",&position[j][0],&position[j][1]);
             if (position[j][0]>=max_x)
                 max_x=position[j][0];
              if (position[j][1]>=max_y)
                 max_y=position[j][1];
         }
      
printf("The new video is %d X %d\n",max_x,max_y);
num_tile_rows_minus1=max_x-1; num_tile_columns_minus1=max_y-1;
///////////////////////////parse all the SPS and VPS//////////////////////////////////////////////
     u32 number_of_CTU_per_width[num_video];
     u32 number_of_CTU_per_height[num_video];
     char *buffer = NULL;
     char *buffer_swap;
     char *PPS[argc];
     GF_BitStream *bs[argc];
     GF_BitStream *bs_tmp[argc];
     GF_BitStream *bs_swap = gf_bs_from_file(file_output, GF_BITSTREAM_WRITE);
     u32 nal_start_code;
     u32 nal_length;
     u32 nal_length_swap;
     int is_nal = 0;
     int width[num_video],height[num_video];
     u8 nal_unit_type =1,temporal_id, layer_id;	
     PPS[0]=NULL;


     for(k=1;k<=num_video;k++)
               {
                  bs[k] = gf_bs_from_file(video[k], GF_BITSTREAM_READ);
                                     
                      if (bs[k] != NULL)
                          { //printf("%d\n",k);
                           HEVCState hevc;									
                           						
                           while(gf_bs_get_size(bs[k])!= gf_bs_get_position(bs[k]))  
                                { 
				 nal_length = gf_media_nalu_next_start_code_bs(bs[k]);
				 gf_bs_skip_bytes(bs[k], nal_length);
				 nal_start_code = gf_bs_read_int(bs[k], 24);
					if(0x000001 == nal_start_code)
					{
					  is_nal = 1;
					}
					   else
					    {
					      if(0x000000 == nal_start_code && 1==gf_bs_read_int(bs[k], 8))
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
                                                  nal_length = gf_media_nalu_next_start_code_bs(bs[k]);
						  if(nal_length == 0)
						  {
							nal_length = gf_bs_get_size(bs[k]) - gf_bs_get_position(bs[k]);
						  }
                                                  buffer = malloc(sizeof(char)*nal_length);
                                                  
                                                  gf_bs_read_data(bs[k],buffer,nal_length);
                                                  bs_tmp[k] = gf_bs_new(buffer, nal_length+1, GF_BITSTREAM_READ);
                                                  gf_media_hevc_parse_nalu(bs_tmp[k], &hevc, &nal_unit_type, &temporal_id, &layer_id);
                                                     if (nal_unit_type == 33)						
						       {
							//printf("===SPS ===\n");
                                                      // parse_and_print_SPS(buffer, nal_length, &hevc);
         						int id_sps = gf_media_hevc_read_sps(buffer, nal_length, &hevc);
                                                        width[k]=hevc.sps[id_sps].width;
                                                        height[k]=hevc.sps[id_sps].height;
						       }
                                                     else if (nal_unit_type == 34)						
						       {      //printf("====================PPS===================");
                                                              //parse_and_print_PPS(buffer, nal_length, &hevc);
                                                              PPS[k] = malloc(sizeof(char)*nal_length); 
                                                              strcpy(PPS[k],buffer);
                                                             if(k>1) 
                                                                {if(PPS[k]!=NULL){
                                                                                 if(memcmp(PPS[k],PPS[k-1],nal_length)) 
                                                                                    {printf("Error:PPS are not same!!!\n");
                                                                                     combination_enabled_flag=0;
                                                                                    }
                                                                                 }
                                                                }


                                                       
                                                      int id_pps = gf_media_hevc_read_pps(buffer, nal_length, &hevc);
                                                      //printf("max_CU_width===%d\n",hevc.sps[hevc.pps[id_pps].sps_id].max_CU_width);
                                                      number_of_CTU_per_width[k]= (width[k]+hevc.sps[hevc.pps[id_pps].sps_id].max_CU_width -1)/hevc.sps[hevc.pps[id_pps].sps_id].max_CU_width;
                                                     
                                                      //printf("number_of_CTU_per_width[k]===%d\n",number_of_CTU_per_width[k]);
                                                      number_of_CTU_per_height[k]=(height[k]+hevc.sps[hevc.pps[id_pps].sps_id].max_CU_height -1)/hevc.sps[hevc.pps[id_pps].sps_id].max_CU_height;
                                                          if (hevc.pps[id_pps].tiles_enabled_flag != 0)
                                                           {printf("Error:The video %d is tiled !!!\n",k);// tiles_enabled_flag ==0!
							    combination_enabled_flag=0; 
                                                           }//printf("===PPS ===\n");
						       }
                                                     
							else;	 
                                               }	
				} 
			 }
                gf_bs_seek(bs[k], 0);
                }

/*for(i=1;i<=num_video;i++)
{
printf("number_of_CTU_per_width[%d]=%d\n",i,number_of_CTU_per_width[i]);
printf("number_of_CTU_per_height[%d]=%d\n",i,number_of_CTU_per_height[i]);

}
*/

int num_CTU_width[max_y],num_CTU_height[max_x];
int num_CTU_width_tot=0,num_CTU_height_tot=0,pic_width=0,pic_height=0;

int x,y,width_ref[max_y],height_ref[max_x];//the reference of width and height
    for(x=1;x<=max_x;x++)
       {
          for(i=1;i<=num_video;i++)
            {   if (position[i][0]==x)//on the same line
                   {//printf("x=%d,position=%d\n",x,position[i][0]);
                    height_ref[x]=height[i];
                    num_CTU_height[x]=number_of_CTU_per_height[i];
                    num_CTU_height_tot=num_CTU_height_tot+num_CTU_height[x];
                    pic_height+=height_ref[x];
                    //printf("height_ref[%d]==%d\n",x,num_CTU_height[x]);
                    //printf("i===%d\n",i);
                    break;
                   }
            }    
        }   

for(y=1;y<=max_y;y++)
    {
       for(i=1;i<=num_video;i++)
       { if (position[i][1]==y)//in the same row
            {
             width_ref[y]=width[i];
             num_CTU_width[y]=number_of_CTU_per_width[i];
             num_CTU_width_tot=num_CTU_width_tot+num_CTU_width[y];
             pic_width+=width_ref[y];
             break;
            }
       }       
    }
//////////////////////////////////////////////////////////////////////////////////check the compatibility of width and height////////////////////////
 for(x=1;x<=max_x;x++)
       {  //printf("height_ref[%d]=%d\n",x,height_ref[x]);
          for(i=1;i<=num_video;i++)
            {
                if (position[i][0]==x)//on the same line
                    { // printf("height[%d]=%d\n",i,height[i]);
                       if(height_ref[x]!=height[i])
                       {//printf("Error: The height of video %d is not compatible!!!\n",i);
                        printf("Error: The height of video is not compatible!!!\n");
                        combination_enabled_flag=0;
                        //break;
                       }
                    }
            }         
       }   
for(y=1;y<=max_y;y++)
    {
       for(i=1;i<=num_video;i++)
         {
               if (position[i][1]==y)//on the same line
                    {
                       if(width_ref[y]!=width[i])
                       {//printf("Error: The width of video %d is not compatible!!!\n",i);
                        printf("Error: The width of video is not compatible!!!\n");
                        combination_enabled_flag=0;
                        //break;
                       }
                    }
         }       
    }
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

switch(combination_enabled_flag)
       {
        case 0: printf("======Error:Combination impossible=====\n");break;
        case 1: printf("======Combination possible=====\n");break;
       }



//printf("width=%d\n",pic_width);
//printf("height=%d\n",pic_height);
//printf("tot_height=%d\n",num_CTU_height_tot);
//int ad;
//ad=new_address(2,2,num_CTU_width,num_CTU_height,num_CTU_width_tot);
//printf("new_adress==%d\n",ad);

 
////////////////////////////////////////////////////////////////////////////////////////combination//////////////////////////////////
int uniform_spacing_flag=0,address=0;
for(k=1;k<=num_video;k++)
{
if (bs[k] != NULL)
                          { 
                           HEVCState hevc;									
                           						
                           while(gf_bs_get_size(bs[k])!= gf_bs_get_position(bs[k]))  
                                { 
				 nal_length = gf_media_nalu_next_start_code_bs(bs[k]);
				 gf_bs_skip_bytes(bs[k], nal_length);
				 nal_start_code = gf_bs_read_int(bs[k], 24);
					if(0x000001 == nal_start_code)
					{
					  is_nal = 1;
                                          gf_bs_write_int(bs_swap, 0x000001, 24);
					}
					   else
					    {
					      if(0x000000 == nal_start_code && 1==gf_bs_read_int(bs[k], 8))
							{
							  is_nal = 1;
                                                          gf_bs_write_int(bs_swap, 0x00000001, 32);
							}
						   else
     						          {				
                                                           is_nal = 0;
							  }
					    }
                                 	       if(is_nal)
                                               {
                                                  nal_length = gf_media_nalu_next_start_code_bs(bs[k]);
						  if(nal_length == 0)
						  {
							nal_length = gf_bs_get_size(bs[k]) - gf_bs_get_position(bs[k]);
						  }
                                                  buffer = malloc(sizeof(char)*nal_length);
                                                  
                                                  gf_bs_read_data(bs[k],buffer,nal_length);
                                                  bs_tmp[k] = gf_bs_new(buffer, nal_length+1, GF_BITSTREAM_READ);
                                                  gf_media_hevc_parse_nalu(bs_tmp[k], &hevc, &nal_unit_type, &temporal_id, &layer_id);
                                                     
                                                 switch(nal_unit_type)
                                                   {
                                                   case 32:
						           printf("===VPS #===\n");
                                                           if(k==1)
                                                           gf_bs_write_data(bs_swap, buffer, nal_length);//copy VPS
                                                           break;
							

                                                      
                                                   case 33:
                                                           printf("===SPS #===\n");
                                                          if(k==1)
                                                           {
                                                           rewrite_SPS_cb(pic_width,pic_height,buffer, nal_length, &buffer_swap, &nal_length_swap, &hevc);
                                                           gf_bs_write_data(bs_swap, buffer_swap, nal_length_swap);
                                                           parse_print_SPS(buffer_swap, nal_length_swap, &hevc);
                                                           free(buffer_swap);
                                                           }
                                                           
                                                           break;
                                                     
                                                       
         						
						       
                                                   case 34:
                                                           printf("===PPS #===\n"); 
                                                         if(k==1)
                                                          {
                                                           rewrite_PPS_cb(num_tile_columns_minus1,num_tile_rows_minus1,uniform_spacing_flag,num_CTU_width,num_CTU_height,buffer, nal_length, &buffer_swap, &nal_length_swap);///problems minus1!!!!!
                                                           gf_bs_write_data(bs_swap, buffer_swap, nal_length_swap);
                                                           parse_print_SPS(buffer_swap, nal_length_swap, &hevc);
                                                           free(buffer_swap);
                                                           }
                                                           
                                                           break;
                                                   default:
                                                           if(nal_unit_type <= 21)  //Slice
							{ 
                                                          address=new_address(position[k][0],position[k][1],num_CTU_width,num_CTU_height,num_CTU_width_tot);
                                                          //printf("x==%d,y==%d\n",position[k][0],position[k][1]);
                                                          //printf("new_address==%d\n",address);
                                                          rewrite_slice_address(address, buffer, nal_length, &buffer_swap, &nal_length_swap, &hevc);
                                                          gf_bs_write_data(bs_swap, buffer_swap, nal_length_swap);
                                                          free(buffer_swap);  
                                                        }

                                                           else
                                                               gf_bs_write_data(bs_swap, buffer, nal_length);
                                                          


                                                          break;
                                                   }     
                                               }	
				} 
			 }



}      

fclose(file_output);
 return 0;
}


int main (int argc, char **argv)
{
    if(argc >= 2)
    {
        if(0==strcmp(argv[1],"--extract") || 0==strcmp(argv[1],"-x"))
        {
            struct stat st = {0};
            if (stat(argv[3], &st) == -1)
                mkdir(argv[3], 0777);
            extract_tiles(argc, argv);
        }
        else if(0==strcmp(argv[1],"--combine") || 0==strcmp(argv[1],"-c"))
        {
            combine_tiles(argc, argv);
        } 
        else if(0==strcmp(argv[1],"--swap") || 0==strcmp(argv[1],"-s"))
            swap_tiles (argc, argv);
        else if(0==strcmp(argv[1],"--info") || 0==strcmp(argv[1],"-i"))
            bs_info(argc, argv);
        else
        {
            printf(" \tThis commands allows to extract the for a given tiled bitstream\n");
            printf(" \tType ./TILES -x filename dir_where_to_save_the_extract_tiles\n");
            printf(" \tType ./TILES --extract filename dir_where_to_save_the_extract_tiles\n");
            
            printf(" \tThis commands allows to combine tiles \n");
            printf(" \tType ./TILES -c file1 file2 ..\n");
            printf(" \tType ./TILES --combine file1 file2 ..\n");
            
            printf(" \tThis commands swap tiles \n");
            printf(" \tType ./TILES -s sourcefile til_1_num til_2_num destfile\n");
            printf(" \tType ./TILES -s sourcefile til_1_num til_2_num destfile\n");
            
            printf(" \tThis commands allows to get info from some bitstream \n");
            printf(" \tType ./TILES -i filename\n");
            printf(" \tType ./TILES --info filename \n");
        }
    }
    else
        {
            printf(" \tThis commands allows to extract the for a given tiled bitstream\n");
            printf(" \tType ./TILES -x filename dir_where_to_save_the_extract_tiles\n");
            printf(" \tType ./TILES --extract filename dir_where_to_save_the_extract_tiles\n");
            
            printf(" \tThis commands allows to combine tiles \n");
            printf(" \tType ./TILES -c file1 file2 ..\n");
            printf(" \tType ./TILES --combine file1 file2 ..\n");
            
            printf(" \tThis commands swap tiles \n");
            printf(" \tType ./TILES -s sourcefile til_1_num til_2_num destfile\n");
            printf(" \tType ./TILES -s sourcefile til_1_num til_2_num destfile\n");
            
            printf(" \tThis commands allows to get info from some bitstream \n");
            printf(" \tType ./TILES -i filename\n");
            printf(" \tType ./TILES --info filename \n");
        }
    //test();
    return 0;
}
