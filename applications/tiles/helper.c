
#include <stdlib.h>
#include <stdio.h>
#include <gpac/bitstream.h>
#include <gpac/internal/media_dev.h>
#include <math.h>
#include "helper.h"

/**
 * Visualization of VPS properties and return VPS ID
     * 
     * @param buffer
     * @param bz
     * @param hevc
     * @return 
     */
u32 parse_print_VPS(char *buffer, u32 bz, HEVCState* hevc){
	u32 i = gf_media_hevc_read_vps(buffer,bz, hevc);
	printf("=== Visualization of VPS with id: %d ===\n",(*hevc).vps[i].id);
	printf("num_profile_tier_level:	%hhu\n",(*hevc).vps[i].num_profile_tier_level);
        return i;
}

/**
 * Visualization of SPS properties and return SPS ID
     * 
     * @param buffer
     * @param bz
     * @param hevc
     * @return 
     */
u32 parse_print_SPS(char *buffer, u32 bz, HEVCState* hevc){
    
	u32 i = gf_media_hevc_read_sps(buffer,bz, hevc);
	printf("=== Visualization of SPS with id: %d ===\n",(*hevc).sps[i].id);
       	printf("width:	%u\n",(*hevc).sps[i].width);
	printf("height:	%u\n",(*hevc).sps[i].height);
        printf("pic_width_in_luma_samples:	%u\n",(*hevc).sps[i].max_CU_width);
        printf("pic_heigth_in_luma_samples:	%u\n",(*hevc).sps[i].max_CU_height);
        printf("cw_left:	%u\n",(*hevc).sps[i].cw_left);
        printf("cw_right:	%u\n",(*hevc).sps[i].cw_right);
        printf("cw_top:	%u\n",(*hevc).sps[i].cw_top);
        printf("cw_bottom:	%u\n",(*hevc).sps[i].cw_bottom);
        return i;
}

/**
 * Visualization of PPS properties and return PPS ID
 * @param buffer
 * @param bz
 * @param hevc
 * @return 
 */
u32 parse_print_PPS(char *buffer, u32 bz, HEVCState* hevc){
	u32 i = gf_media_hevc_read_pps(buffer,bz, hevc);
	printf("=== Visualization of PPS with id: %d ===\n",(*hevc).pps[i].id);
     	printf("tiles_enabled_flag:	%u\n",(*hevc).pps[i].tiles_enabled_flag);
     	printf("uniform_spacing_flag:	%d\n",(*hevc).pps[i].uniform_spacing_flag);
	printf("num_tile_columns:	%u\n",(*hevc).pps[i].num_tile_columns);
	printf("num_tile_rows:	%u\n",(*hevc).pps[i].num_tile_rows);
        return i;
}

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

void bs_set_ue(GF_BitStream *bs, u32 num) {
    s32 length = 1;
    s32 temp = ++num;

    while (temp != 1) {
        temp >>= 1;
        length += 2;
    }
    
    gf_bs_write_int(bs, 0, length >> 1);
    gf_bs_write_int(bs, num, (length+1) >> 1);
}

void bs_set_se(GF_BitStream *bs,s32 num)
{	u32 v;
	if(num <=0)
		v = (-1*num)<<1;
	else
		v = (num << 1) - 1;

	bs_set_ue(bs,v);
}
//GF_EXPORT
static u32 avc_emulation_bytes_add_count(char *buffer, u32 nal_size)
{
	u32 i = 0, emulation_bytes_count = 0;
	u8 num_zero = 0;

	while (i < nal_size) {
		/*ISO 14496-10: "Within the NAL unit, any four-byte sequence that starts with 0x000003
		other than the following sequences shall not occur at any byte-aligned position:
		\96 0x00000300
		\96 0x00000301
		\96 0x00000302
		\96 0x00000303"
		*/
		if (num_zero == 2 && buffer[i] < 0x04) {
			/*emulation code found*/
			num_zero = 0;
			emulation_bytes_count++;
			if (!buffer[i])
				num_zero = 1;
		} else {
			if (!buffer[i])
				num_zero++;
			else
				num_zero = 0;
		}
		i++;
	}
	return emulation_bytes_count;
}

static u32 avc_add_emulation_bytes(const char *buffer_src, char *buffer_dst, u32 nal_size)
{

	u32 i = 0, emulation_bytes_count = 0;
	u8 num_zero = 0;

	while (i < nal_size) {
		/*ISO 14496-10: "Within the NAL unit, any four-byte sequence that starts with 0x000003
		other than the following sequences shall not occur at any byte-aligned position:
		0x00000300
		0x00000301
		0x00000302
		0x00000303"
		*/
		if (num_zero == 2 && buffer_src[i] < 0x04) {
			/*add emulation code*/
			num_zero = 0;
			buffer_dst[i+emulation_bytes_count] = 0x03;
			emulation_bytes_count++;
			if (!buffer_src[i])
				num_zero = 1;
		} else {
			if (!buffer_src[i])
				num_zero++;
			else
				num_zero = 0;
		}
		buffer_dst[i+emulation_bytes_count] = buffer_src[i];
		i++;
	}
	return nal_size+emulation_bytes_count;
}

static u32 avc_remove_emulation_bytes(const char *buffer_src, char *buffer_dst, u32 nal_size)
{
	u32 i = 0, emulation_bytes_count = 0;
	u8 num_zero = 0;

	while (i < nal_size)
	{
		/*ISO 14496-10: "Within the NAL unit, any four-byte sequence that starts with 0x000003
		  other than the following sequences shall not occur at any byte-aligned position:
		  0x00000300
		  0x00000301
		  0x00000302
		  0x00000303"
		*/
		if (num_zero == 2
		        && buffer_src[i] == 0x03
		        && i+1 < nal_size /*next byte is readable*/
		        && buffer_src[i+1] < 0x04)
		{
			/*emulation code found*/
			num_zero = 0;
			emulation_bytes_count++;
			i++;
		}

		buffer_dst[i-emulation_bytes_count] = buffer_src[i];

		if (!buffer_src[i])
			num_zero++;
		else
			num_zero = 0;

		i++;
	}

	return nal_size-emulation_bytes_count;
}

u32 new_address(int x,int y,int num_CTU_height[],int num_CTU_width[],int num_CTU_width_tot)
{ int sum_height=0,sum_width=0,adress=0;
  int i,j;
  for(i=1;i<x;i++) sum_height+=num_CTU_height[i];
  for(j=1;j<y;j++) sum_width+=num_CTU_width[j];
  adress=sum_height*num_CTU_width_tot+sum_width;
    
//adress= sum_height;
return adress;
}

//Transform slice 1d address into 2D address
void get_2D_cordinates_of_slices(u32 *x, u32 *y, HEVCState *hevc)
{
    *x = 0;
    *y = 0;
    u32 number_of_CTU_per_width = (hevc->s_info.sps->width + hevc->s_info.sps->max_CU_width -1) / hevc->s_info.sps->max_CU_width;
    
    u32 index_x = hevc->s_info.slice_segment_address % number_of_CTU_per_width;
    u32 index_y = hevc->s_info.slice_segment_address /number_of_CTU_per_width;
    
    if(hevc->s_info.pps->tiles_enabled_flag)
    {
        if(hevc->s_info.pps->uniform_spacing_flag)
        {
            *x = ((index_x+1) * hevc->s_info.sps->max_CU_width)/(hevc->s_info.sps->width/(hevc->s_info.pps->num_tile_columns));
            *y = ((index_y+1) * hevc->s_info.sps->max_CU_height)/(hevc->s_info.sps->height/(hevc->s_info.pps->num_tile_rows));
        }   
        else
        {
            u32 cum_width=0;
            u32 cum_heigth=0, i;
            for(i=0;i < hevc->s_info.pps->num_tile_columns;i++)
            {
                if(index_x == cum_width)
                {
                    *x = i;
                    break;
                }
                cum_width += hevc->s_info.pps->column_width[i];
            }
            for(i=0;i < hevc->s_info.pps->num_tile_rows;i++)
            {
                if(index_y == cum_heigth)
                {
                    *y = i;
                    break;
                } 
                cum_heigth += hevc->s_info.pps->row_height[i]; 
            } 
        }
    }
}


//get the width and the height of the tile in pixel
void get_size_of_slice(u32 *width, u32 *height, HEVCState *hevc)
{
    u32 number_of_CTU_on_width = (hevc->s_info.sps->width + hevc->s_info.sps->max_CU_width -1) / hevc->s_info.sps->max_CU_width;
    u32 number_of_CTU_on_height = (hevc->s_info.sps->height + hevc->s_info.sps->max_CU_height -1) / hevc->s_info.sps->max_CU_height;
    
    u32 index_x = hevc->s_info.slice_segment_address % number_of_CTU_on_width;
    u32 index_y = hevc->s_info.slice_segment_address /number_of_CTU_on_width;
    
    if(hevc->s_info.pps->tiles_enabled_flag)
    {
        if(hevc->s_info.pps->uniform_spacing_flag)
        {
            u32 w = number_of_CTU_on_width/hevc->s_info.pps->num_tile_columns;
            if((number_of_CTU_on_width % hevc->s_info.pps->num_tile_columns) != 0)
                w +=1;
            *width = w * hevc->s_info.sps->max_CU_width;
            u32 h = number_of_CTU_on_height/hevc->s_info.pps->num_tile_rows;
            if((number_of_CTU_on_height % hevc->s_info.pps->num_tile_rows) != 0)
                h +=1;
            *height = h * hevc->s_info.sps->max_CU_height;
        }   
        else
        {
            u32 cum_width=0;
            u32 cum_heigth=0, i;
            for(i=0;i< hevc->s_info.pps->num_tile_columns;i++)
            {
                if(index_x == cum_width)
                {
                    *width = hevc->s_info.pps->column_width[i] * hevc->s_info.sps->max_CU_width;
                    break;
                }
                cum_width += hevc->s_info.pps->column_width[i];
            }
            for(i=0;i< hevc->s_info.pps->num_tile_rows;i++)
            {
                if(index_y == cum_heigth)
                {
                    *height = hevc->s_info.pps->row_height[i] * hevc->s_info.sps->max_CU_height; 
                    break;
                } 
                cum_heigth += hevc->s_info.pps->row_height[i]; 
            } 
        }    
    }
}

//rewrite the profile and level
void write_profile_tier_level(GF_BitStream *bs_in, GF_BitStream *bs_out , Bool ProfilePresentFlag, u8 MaxNumSubLayersMinus1)
{
    u8 j;
    Bool sub_layer_profile_present_flag[8], sub_layer_level_present_flag[8];
    if (ProfilePresentFlag) {
        gf_bs_write_int(bs_out, gf_bs_read_int(bs_in, 8), 8);
        gf_bs_write_long_int(bs_out, gf_bs_read_long_int(bs_in, 32), 32);
        gf_bs_write_int(bs_out, gf_bs_read_int(bs_in, 4), 4);
        gf_bs_write_long_int(bs_out, gf_bs_read_long_int(bs_in, 44), 44);
    }
    gf_bs_write_int(bs_out, gf_bs_read_int(bs_in, 8), 8);
    
    for (j=0; j < MaxNumSubLayersMinus1; j++) {
        sub_layer_profile_present_flag[j] = gf_bs_read_int(bs_in, 1);
        gf_bs_write_int(bs_out, sub_layer_profile_present_flag[j], 1);
        sub_layer_level_present_flag[j] = gf_bs_read_int(bs_in, 1);
        gf_bs_write_int(bs_out,sub_layer_level_present_flag[j], 1);
    }
    if (MaxNumSubLayersMinus1 > 0)
        for (j=MaxNumSubLayersMinus1; j < 8; j++) 
            gf_bs_write_int(bs_out, gf_bs_read_int(bs_in, 2), 2);
    

    for (j=0; j<MaxNumSubLayersMinus1; j++) {
        if (sub_layer_profile_present_flag[j]) {
            gf_bs_write_int(bs_out, gf_bs_read_int(bs_in, 8), 8);
            gf_bs_write_int(bs_out, gf_bs_read_int(bs_in, 32), 32);
            gf_bs_write_int(bs_out, gf_bs_read_int(bs_in, 4), 4);
            gf_bs_write_long_int(bs_out, gf_bs_read_long_int(bs_in, 44), 44);
        }
        if( sub_layer_level_present_flag[j])
            gf_bs_write_int(bs_out, gf_bs_read_int(bs_in, 8), 8);
    }
}

void rewrite_SPS(char *in_SPS, u32 in_SPS_length, char **out_SPS, u32 *out_SPS_length, HEVCState *hevc)
{
    GF_BitStream *bs_in, *bs_out;
    u64 length_no_use = 0;
    char *data_without_emulation_bytes = NULL;
    u32 data_without_emulation_bytes_size = 0, sps_ext_or_max_sub_layers_minus1;
    u8 max_sub_layers_minus1=0, layer_id;
    Bool conformance_window_flag, multiLayerExtSpsFlag;;
    u32 chroma_format_idc, width, height;
    
    data_without_emulation_bytes = gf_malloc(in_SPS_length*sizeof(char));
    data_without_emulation_bytes_size = avc_remove_emulation_bytes(in_SPS, data_without_emulation_bytes, in_SPS_length);
    bs_in = gf_bs_new(data_without_emulation_bytes, data_without_emulation_bytes_size, GF_BITSTREAM_READ);
    bs_out = gf_bs_new(NULL, length_no_use, GF_BITSTREAM_WRITE);
    if (!bs_in) goto exit;
    
    //copy NAL Header
    gf_bs_write_int(bs_out, gf_bs_read_int(bs_in, 7), 7); 
    layer_id = gf_bs_read_int(bs_in, 6);
    gf_bs_write_int(bs_out, layer_id, 6);
    gf_bs_write_int(bs_out, gf_bs_read_int(bs_in, 3), 3);
    
    gf_bs_write_int(bs_out, gf_bs_read_int(bs_in, 4), 4); //copy vps_id
    
    if (layer_id == 0)
    {
        max_sub_layers_minus1 = gf_bs_read_int(bs_in, 3);
        gf_bs_write_int(bs_out, max_sub_layers_minus1, 3);
    }  
    else
    {
        sps_ext_or_max_sub_layers_minus1 = gf_bs_read_int(bs_in, 3);
        gf_bs_write_int(bs_out, sps_ext_or_max_sub_layers_minus1, 3);
    }
    multiLayerExtSpsFlag = (layer_id != 0) && (sps_ext_or_max_sub_layers_minus1 == 7);
    if (!multiLayerExtSpsFlag) {		
        gf_bs_write_int(bs_out, gf_bs_read_int(bs_in, 1), 1);
        write_profile_tier_level(bs_in, bs_out, 1, max_sub_layers_minus1);
    }
    
    bs_set_ue(bs_out,bs_get_ue(bs_in)); //copy sps_id
    
    if (multiLayerExtSpsFlag) {
        u8 update_rep_format_flag = gf_bs_read_int(bs_in, 1);
        gf_bs_write_int(bs_out, update_rep_format_flag, 1);
        if (update_rep_format_flag) {
            gf_bs_write_int(bs_out, gf_bs_read_int(bs_in, 8), 8);
        }
    }else {
        chroma_format_idc = bs_get_ue(bs_in);
        bs_set_ue(bs_out,chroma_format_idc);
        if(chroma_format_idc==3)
            gf_bs_write_int(bs_out, gf_bs_read_int(bs_in, 1), 1); // copy separate_colour_plane_flag
        bs_get_ue(bs_in); //skip width bits in input bitstream
	bs_get_ue(bs_in); //skip height bits in input bitstream
        get_size_of_slice(&width, &height, hevc); // get the width and height of the tile
        
        //Copy the new width and height in output bitstream
        bs_set_ue(bs_out,width);
        bs_set_ue(bs_out,height);
        
        //Get rid of the bit conformance_window_flag
        conformance_window_flag = gf_bs_read_int(bs_in, 1);
        //put the new conformance flag to zero
        gf_bs_write_int(bs_out, 0, 1);
        
        //Skip the bits related to conformance_window_offset
        if(conformance_window_flag)
        {
            bs_get_ue(bs_in);
            bs_get_ue(bs_in);
            bs_get_ue(bs_in);
            bs_get_ue(bs_in);
        }
    }
    
    //Copy the rest of the bitstream
    if(gf_bs_get_bit_position(bs_in) != 8)
        gf_bs_write_int(bs_out, gf_bs_read_int(bs_in, 8-gf_bs_get_bit_position(bs_in)), 8-gf_bs_get_bit_position(bs_in));
    while (gf_bs_get_size(bs_in) != gf_bs_get_position(bs_in)) //Rest contents copying
    {
        gf_bs_write_int(bs_out, gf_bs_read_int(bs_in, 8), 8);      
    }


    gf_bs_align(bs_out);						//align

    *out_SPS_length = 0;
    *out_SPS = NULL;
    char *buffer;
    gf_bs_get_content(bs_out, &buffer, out_SPS_length);
    u32 length = *out_SPS_length + avc_emulation_bytes_add_count(buffer,*out_SPS_length);
    *out_SPS = gf_malloc(length*sizeof(char));
    avc_add_emulation_bytes(buffer,*out_SPS,*out_SPS_length);
    *out_SPS_length = length;  

exit:
	gf_bs_del(bs_in);
	gf_bs_del(bs_out);
	gf_free(data_without_emulation_bytes);
        gf_free(buffer);
}

void rewrite_PPS(char *in_PPS, u32 in_PPS_length, char **out_PPS, u32 *out_PPS_length)
{
    u64 length_no_use = 0;
    u8 cu_qp_delta_enabled_flag, tiles_enabled_flag;
    
    GF_BitStream *bs_in ;
    GF_BitStream *bs_out;
    
    char *data_without_emulation_bytes = NULL;
    u32 data_without_emulation_bytes_size = 0;
    data_without_emulation_bytes = gf_malloc(in_PPS_length*sizeof(char));
    data_without_emulation_bytes_size = avc_remove_emulation_bytes(in_PPS, data_without_emulation_bytes, in_PPS_length);
    bs_in = gf_bs_new(data_without_emulation_bytes, data_without_emulation_bytes_size, GF_BITSTREAM_READ);
    bs_out = gf_bs_new(NULL, length_no_use, GF_BITSTREAM_WRITE);
    if (!bs_in) goto exit;
    
    //Read and write NAL header bits
    gf_bs_write_int(bs_out, gf_bs_read_int(bs_in, 16), 16);
    bs_set_ue(bs_out, bs_get_ue(bs_in)); //pps_pic_parameter_set_id
    bs_set_ue(bs_out, bs_get_ue(bs_in)); //pps_seq_parameter_set_id
    gf_bs_write_int(bs_out, gf_bs_read_int(bs_in, 7), 7); //from dependent_slice_segments_enabled_flag to cabac_init_present_flag
    bs_set_ue(bs_out, bs_get_ue(bs_in)); //num_ref_idx_l0_default_active_minus1
    bs_set_ue(bs_out, bs_get_ue(bs_in)); //num_ref_idx_l1_default_active_minus1
    bs_set_se(bs_out, bs_get_se(bs_in)); //init_qp_minus26
    gf_bs_write_int(bs_out, gf_bs_read_int(bs_in, 2), 2); //from constrained_intra_pred_flag to transform_skip_enabled_flag
    cu_qp_delta_enabled_flag = gf_bs_read_int(bs_in, 1); //cu_qp_delta_enabled_flag
    gf_bs_write_int(bs_out, cu_qp_delta_enabled_flag, 1); //
    if(cu_qp_delta_enabled_flag) 
        bs_set_ue(bs_out, bs_get_ue(bs_in)); // diff_cu_qp_delta_depth
    bs_set_se(bs_out, bs_get_se(bs_in)); // pps_cb_qp_offset
    bs_set_se(bs_out, bs_get_se(bs_in)); // pps_cr_qp_offset
    gf_bs_write_int(bs_out, gf_bs_read_int(bs_in, 4), 4); // from pps_slice_chroma_qp_offsets_present_flag to transquant_bypass_enabled_flag
    tiles_enabled_flag = gf_bs_read_int(bs_in, 1); // tiles_enabled_flag
    gf_bs_write_int(bs_out, 0, 1);
    gf_bs_write_int(bs_out, gf_bs_read_int(bs_in, 1), 1); // entropy_coding_sync_enabled_flag
    if(tiles_enabled_flag)
     {
        u32 num_tile_columns_minus1 = bs_get_ue(bs_in);
        u32 num_tile_rows_minus1 = bs_get_ue(bs_in);
        u8 uniform_spacing_flag =  gf_bs_read_int(bs_in, 1);
        if(!uniform_spacing_flag)
        {
            u32 i;
            for(i=0;i<num_tile_columns_minus1;i++)
                bs_get_ue(bs_in);
            for(i=0;i<num_tile_rows_minus1;i++)
                bs_get_ue(bs_in);
        } 
        gf_bs_read_int(bs_in, 1);
    }
    
    //copy and write the rest of the bits
    if(gf_bs_get_bit_position(bs_in) != 8)
        gf_bs_write_int(bs_out, gf_bs_read_int(bs_in, 8-gf_bs_get_bit_position(bs_in)), 8-gf_bs_get_bit_position(bs_in));
    while (gf_bs_get_size(bs_in) != gf_bs_get_position(bs_in)) //Rest contents copying
    {
        gf_bs_write_int(bs_out, gf_bs_read_int(bs_in, 8), 8);      
    }
    
    gf_bs_align(bs_out);						//align

    *out_PPS_length = 0;
    *out_PPS = NULL;
    char *buffer;
    gf_bs_get_content(bs_out, &buffer, out_PPS_length);
    u32 length = *out_PPS_length + avc_emulation_bytes_add_count(buffer,*out_PPS_length);
    *out_PPS = gf_malloc(length*sizeof(char));
    avc_add_emulation_bytes(buffer,*out_PPS,*out_PPS_length);
    *out_PPS_length = length;  
    
    exit:
	gf_bs_del(bs_in);
	gf_bs_del(bs_out);
        gf_free(data_without_emulation_bytes);
        gf_free(buffer);
}


void rewrite_SPS_cb(u32 pic_width, u32 pic_height,char *in_SPS, u32 in_SPS_length, char **out_SPS, u32 *out_SPS_length, HEVCState *hevc)
{
    GF_BitStream *bs_in, *bs_out;
    u64 length_no_use = 0;
    char *data_without_emulation_bytes = NULL;
    u32 data_without_emulation_bytes_size = 0, sps_ext_or_max_sub_layers_minus1;
    u8 max_sub_layers_minus1, layer_id;
    Bool conformance_window_flag, multiLayerExtSpsFlag;;
    u32 chroma_format_idc, width, height;
    
    data_without_emulation_bytes = gf_malloc(in_SPS_length*sizeof(char));
    data_without_emulation_bytes_size = avc_remove_emulation_bytes(in_SPS, data_without_emulation_bytes, in_SPS_length);
    bs_in = gf_bs_new(data_without_emulation_bytes, data_without_emulation_bytes_size, GF_BITSTREAM_READ);
    bs_out = gf_bs_new(NULL, length_no_use, GF_BITSTREAM_WRITE);
    if (!bs_in) goto exit;
    
    //copy NAL Header
    gf_bs_write_int(bs_out, gf_bs_read_int(bs_in, 7), 7); 
    layer_id = gf_bs_read_int(bs_in, 6);
    gf_bs_write_int(bs_out, layer_id, 6);
    gf_bs_write_int(bs_out, gf_bs_read_int(bs_in, 3), 3);
    
    gf_bs_write_int(bs_out, gf_bs_read_int(bs_in, 4), 4); //copy vps_id
    
    if (layer_id == 0)
    {
        max_sub_layers_minus1 = gf_bs_read_int(bs_in, 3);
        gf_bs_write_int(bs_out, max_sub_layers_minus1, 3);
    }  
    else
    {
        sps_ext_or_max_sub_layers_minus1 = gf_bs_read_int(bs_in, 3);
        gf_bs_write_int(bs_out, sps_ext_or_max_sub_layers_minus1, 3);
    }
    multiLayerExtSpsFlag = (layer_id != 0) && (sps_ext_or_max_sub_layers_minus1 == 7);
    if (!multiLayerExtSpsFlag) {		
        gf_bs_write_int(bs_out, gf_bs_read_int(bs_in, 1), 1);
        write_profile_tier_level(bs_in, bs_out, 1, max_sub_layers_minus1);
    }
    
    bs_set_ue(bs_out,bs_get_ue(bs_in)); //copy sps_id
    
    if (multiLayerExtSpsFlag) {
        u8 update_rep_format_flag = gf_bs_read_int(bs_in, 1);
        gf_bs_write_int(bs_out, update_rep_format_flag, 1);
        if (update_rep_format_flag) {
            gf_bs_write_int(bs_out, gf_bs_read_int(bs_in, 8), 8);
        }
    }else {
        chroma_format_idc = bs_get_ue(bs_in);
        bs_set_ue(bs_out,chroma_format_idc);
        if(chroma_format_idc==3)
            gf_bs_write_int(bs_out, gf_bs_read_int(bs_in, 1), 1); // copy separate_colour_plane_flag
        bs_get_ue(bs_in);
	bs_get_ue(bs_in);
        //get_wh_of_slice(&width, &height, pps_id, hevc);   
        width=pic_width; 
        height=pic_height;
        bs_set_ue(bs_out,width);   
        bs_set_ue(bs_out,height);
        
        conformance_window_flag = gf_bs_read_int(bs_in, 1);
        gf_bs_write_int(bs_out, 0, 1);
        if(conformance_window_flag)
        {
            bs_get_ue(bs_in);
            bs_get_ue(bs_in);
            bs_get_ue(bs_in);
            bs_get_ue(bs_in);
        }
    }
    
    gf_bs_write_int(bs_out, gf_bs_read_int(bs_in, 8-gf_bs_get_bit_position(bs_in)), 8-gf_bs_get_bit_position(bs_in));
    while (gf_bs_get_size(bs_in) != gf_bs_get_position(bs_in)) //Rest contents copying
    {
        gf_bs_write_int(bs_out, gf_bs_read_int(bs_in, 8), 8);      
    }


    gf_bs_align(bs_out);						//align

    *out_SPS_length = 0;
    *out_SPS = NULL;
    char *buffer;
    gf_bs_get_content(bs_out, &buffer, out_SPS_length);
    u32 length = *out_SPS_length + avc_emulation_bytes_add_count(buffer,*out_SPS_length);
    *out_SPS = gf_malloc(length*sizeof(char));
    avc_add_emulation_bytes(buffer,*out_SPS,*out_SPS_length);
    *out_SPS_length = length;  
    
    exit:
	gf_bs_del(bs_in);
	gf_bs_del(bs_out);
        gf_free(data_without_emulation_bytes);
        gf_free(buffer);
}
///////////

void rewrite_PPS_cb(u32 num_tile_columns_minus1,u32 num_tile_rows_minus1,u32 uniform_spacing_flag,u32 column_width_minus1[],u32 row_height_minus1[],char *in_PPS, u32 in_PPS_length, char **out_PPS, u32 *out_PPS_length)
{
    u64 length_no_use = 0;
    u8 cu_qp_delta_enabled_flag, tiles_enabled_flag;
    
    GF_BitStream *bs_in ;
    GF_BitStream *bs_out ;
    
    char *data_without_emulation_bytes = NULL;
    u32 data_without_emulation_bytes_size = 0;
    data_without_emulation_bytes = gf_malloc(in_PPS_length*sizeof(char));
    data_without_emulation_bytes_size = avc_remove_emulation_bytes(in_PPS, data_without_emulation_bytes, in_PPS_length);
    bs_in = gf_bs_new(data_without_emulation_bytes, data_without_emulation_bytes_size, GF_BITSTREAM_READ);
    bs_out = gf_bs_new(NULL, length_no_use, GF_BITSTREAM_WRITE);
    if (!bs_in) goto exit;
    
    gf_bs_write_int(bs_out, gf_bs_read_int(bs_in, 16), 16);
    bs_set_ue(bs_out, bs_get_ue(bs_in));//pps_id
    bs_set_ue(bs_out, bs_get_ue(bs_in));//sps_id
    gf_bs_write_int(bs_out, gf_bs_read_int(bs_in, 7), 7);
    bs_set_ue(bs_out, bs_get_ue(bs_in));//num_ref_idx_l0_default_active_minus1
    bs_set_ue(bs_out, bs_get_ue(bs_in));//num_ref_idx_l1_default_active_minus1
    bs_set_se(bs_out, bs_get_se(bs_in));//init_qp_minus26
    gf_bs_write_int(bs_out, gf_bs_read_int(bs_in, 2), 2);//constrained_intra_pred_flag,transform_skip_enabled_flag
    cu_qp_delta_enabled_flag = gf_bs_read_int(bs_in, 1);
    gf_bs_write_int(bs_out, cu_qp_delta_enabled_flag, 1);//cu_qp_delta_enabled_flag
    if(cu_qp_delta_enabled_flag)
    bs_set_ue(bs_out, bs_get_ue(bs_in));//diff_cu_qp_delta_depth
    bs_set_se(bs_out, bs_get_se(bs_in));//pps_cb_qp_offset
    bs_set_se(bs_out, bs_get_se(bs_in));//pps_cr_qp_offset
    gf_bs_write_int(bs_out, gf_bs_read_int(bs_in, 4), 4);//pps_slice_chroma_qp_offsets_present_flag-----transquant_bypass_enabled_flag
    
    
    //tile//////////////////////
    
    gf_bs_read_int(bs_in, 1);
    gf_bs_write_int(bs_out, 1, 1);//tiles_enable_flag ----from 0 to 1
    tiles_enabled_flag =1;//always enable
    gf_bs_write_int(bs_out, gf_bs_read_int(bs_in, 1), 1);//entropy_coding_sync_enabled_flag    
    bs_set_ue(bs_out, num_tile_columns_minus1);//write num_tile_columns_minus1 
    bs_set_ue(bs_out, num_tile_rows_minus1);//num_tile_rows_minus1
    gf_bs_write_int(bs_out, uniform_spacing_flag, 1);  //uniform_spacing_flag
       
        if(!uniform_spacing_flag)
        {
            u32 i;
            for(i=0;i<num_tile_columns_minus1;i++)
                bs_set_ue(bs_out,column_width_minus1[i]-1);//column_width_minus1[i]
            for(i=0;i<num_tile_rows_minus1;i++)
                bs_set_ue(bs_out,row_height_minus1[i]-1);//row_height_minus1[i]
        } 
     u32 loop_filter_across_tiles_enabled_flag=1;//loop_filter_across_tiles_enabled_flag
     gf_bs_write_int(bs_out, loop_filter_across_tiles_enabled_flag, 1); 
    
    gf_bs_write_int(bs_out, gf_bs_read_int(bs_in, 8-gf_bs_get_bit_position(bs_in)), 8-gf_bs_get_bit_position(bs_in));
    while (gf_bs_get_size(bs_in) != gf_bs_get_position(bs_in)) //Rest contents copying
    {
        gf_bs_write_int(bs_out, gf_bs_read_int(bs_in, 8), 8);      
    }
    
    gf_bs_align(bs_out);						//align

    *out_PPS_length = 0;
    *out_PPS = NULL;
    char *buffer;
    gf_bs_get_content(bs_out, &buffer, out_PPS_length);
    u32 length = *out_PPS_length + avc_emulation_bytes_add_count(buffer,*out_PPS_length);
    *out_PPS = gf_malloc(length*sizeof(char));
    avc_add_emulation_bytes(buffer,*out_PPS,*out_PPS_length);
    *out_PPS_length = length;  
    
    exit:
	gf_bs_del(bs_in);
	gf_bs_del(bs_out);
        gf_free(data_without_emulation_bytes);
        gf_free(buffer);
}

u32 hevc_get_tile_id(HEVCState *hevc, u32 *tile_x, u32 *tile_y, u32 *tile_width, u32 *tile_height)
{
	HEVCSliceInfo *si = &hevc->s_info;
	u32 i, tbX, tbY, PicWidthInCtbsY, PicHeightInCtbsY, tileX, tileY, oX, oY, val;

	PicWidthInCtbsY = si->sps->width / si->sps->max_CU_width;
	if (PicWidthInCtbsY * si->sps->max_CU_width < si->sps->width) PicWidthInCtbsY++;
	PicHeightInCtbsY = si->sps->height / si->sps->max_CU_width;
	if (PicHeightInCtbsY * si->sps->max_CU_width < si->sps->height) PicHeightInCtbsY++;

	tbX = si->slice_segment_address % PicWidthInCtbsY;
	tbY = si->slice_segment_address / PicWidthInCtbsY;

	tileX = tileY = 0;
	oX = oY = 0;
	for (i=0; i < si->pps->num_tile_columns; i++) {
		if (si->pps->uniform_spacing_flag) {
			val = (i+1)*PicWidthInCtbsY / si->pps->num_tile_columns - (i)*PicWidthInCtbsY / si->pps->num_tile_columns;
		} else {
			if (i<si->pps->num_tile_columns-1) {
				val = si->pps->column_width[i];
			} else {
				val = (PicWidthInCtbsY - si->pps->column_width[i-1]);
			}
		}
		*tile_x = oX;
		*tile_width = val;

		if (oX >= tbX) break;
		oX += val;
		tileX++;
	}
	for (i=0; i<si->pps->num_tile_rows; i++) {
		if (si->pps->uniform_spacing_flag) {
			val = (i+1)*PicHeightInCtbsY / si->pps->num_tile_rows - (i)*PicHeightInCtbsY / si->pps->num_tile_rows;
		} else {
			if (i<si->pps->num_tile_rows-1) {
				val = si->pps->row_height[i];
			} else {
				val = (PicHeightInCtbsY - si->pps->row_height[i-1]);
			}
		}
		*tile_y = oY;
		*tile_height = val;

		if (oY >= tbY) break;
		oY += val;
		tileY++;
	}
	*tile_x = *tile_x * si->sps->max_CU_width;
	*tile_y = *tile_y * si->sps->max_CU_width;
	*tile_width = *tile_width * si->sps->max_CU_width;
	*tile_height = *tile_height * si->sps->max_CU_width;

	if (*tile_x + *tile_width > si->sps->width)
		*tile_width = si->sps->width - *tile_x;
	if (*tile_y + *tile_height > si->sps->height)
		*tile_height = si->sps->height - *tile_y;

	return tileX + tileY * si->pps->num_tile_columns;
}

void slice_address_calculation(HEVCState *hevc, u32 *address, u32 tile_x, u32 tile_y)
{
	HEVCSliceInfo *si = &hevc->s_info;
	u32 PicWidthInCtbsY, PicHeightInCtbsY;
	
	PicWidthInCtbsY = si->sps->width / si->sps->max_CU_width;
	if (PicWidthInCtbsY * si->sps->max_CU_width < si->sps->width) PicWidthInCtbsY++;

	*address = tile_x / si->sps->max_CU_width + (tile_y / si->sps->max_CU_width) * PicWidthInCtbsY;
}


void rewrite_slice_address(u32 new_address, char *in_slice, u32 in_slice_length, char **out_slice, u32 *out_slice_length, HEVCState* hevc) 
{
	char *in_slice_without_emulation_bytes;
	char *out_slice_without_emulation_bytes;
	in_slice_without_emulation_bytes = malloc(sizeof(char)*in_slice_length);
	avc_remove_emulation_bytes(in_slice, in_slice_without_emulation_bytes, in_slice_length);
	GF_BitStream *bs_ori = gf_bs_new(in_slice_without_emulation_bytes, in_slice_length, GF_BITSTREAM_READ);
	//GF_BitStream *bs_ori = gf_bs_new(in_slice, in_slice_length, GF_BITSTREAM_READ);
	GF_BitStream *bs_rw = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
	u32 first_slice_segment_in_pic_flag;
	u32 dependent_slice_segment_flag;
	int address_ori;

	gf_bs_skip_bytes(bs_ori, 2);
	hevc_parse_slice_segment(bs_ori, hevc, &(hevc->s_info));
	u32 header_end = (gf_bs_get_position(bs_ori)-1)*8+gf_bs_get_bit_position(bs_ori);

	gf_bs_seek(bs_ori, 0);

	//nal_unit_header			 
	gf_bs_write_int(bs_rw, gf_bs_read_int(bs_ori, 1), 1);
	u32 nal_unit_type = gf_bs_read_int(bs_ori, 6);
	gf_bs_write_int(bs_rw, nal_unit_type, 6);
	gf_bs_write_int(bs_rw, gf_bs_read_int(bs_ori, 9), 9);
	

	first_slice_segment_in_pic_flag = gf_bs_read_int(bs_ori, 1);    //first_slice_segment_in_pic_flag
	if (new_address == 0)  						
		gf_bs_write_int(bs_rw, 1, 1);
	else
		gf_bs_write_int(bs_rw, 0, 1);
	

	if (nal_unit_type >= 16 && nal_unit_type <= 23)                 //no_output_of_prior_pics_flag  
		gf_bs_write_int(bs_rw, gf_bs_read_int(bs_ori, 1), 1);
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
	else
	{
		dependent_slice_segment_flag = GF_FALSE;
	}
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
	else; //new_address = 0
	
	//printf("ori_pos:%llu   rw_pos:%llu\n", gf_bs_get_position(bs_ori)*8+gf_bs_get_bit_position(bs_ori), gf_bs_get_position(bs_rw)*8+gf_bs_get_bit_position(bs_rw));
	

	while (header_end != (gf_bs_get_position(bs_ori)-1)*8+gf_bs_get_bit_position(bs_ori)) //Copy till the end of the header
	{
		gf_bs_write_int(bs_rw, gf_bs_read_int(bs_ori, 1), 1);
		//printf("bit pos:%d\t",gf_bs_get_bit_position(bs_ori));
	}	
	
	gf_bs_align(bs_rw);					//align
	gf_bs_align(bs_ori);     				//skip the padding 0s in original bitstream
	//printf("in_type:%d rw_address:%d header_end:%d\n",nal_unit_type,new_address,header_end);

	//u32 rest_content_length = gf_bs_get_size(bs_ori)-gf_bs_get_position(bs_ori);
	//gf_bs_write_int(bs_rw, gf_bs_read_int(bs_ori, rest_content_length), rest_content_length);

	while (gf_bs_get_size(bs_ori) != gf_bs_get_position(bs_ori)) //Rest contents copying
	{
		//printf("size: %llu %llu\n", gf_bs_get_size(bs_ori)*8, (gf_bs_get_position(bs_ori)-1)*8+gf_bs_get_bit_position(bs_ori));
		gf_bs_write_int(bs_rw, gf_bs_read_int(bs_ori, 8), 8);
	}
	
	
		
	
	//gf_bs_align(bs_rw);						//align
	gf_bs_truncate(bs_rw);
	//printf("input bs size: %llu\n", gf_bs_get_size(bs_ori));
	//printf("output bs size: %llu\n", gf_bs_get_size(bs_rw));
	//printf("input pos:%lld\n", (gf_bs_get_position(bs_ori)-1)*8+gf_bs_get_bit_position(bs_ori));
	//printf("output pos:%lld\n", (gf_bs_get_position(bs_rw)-1)*8+gf_bs_get_bit_position(bs_rw));
	*out_slice_length = 0;
	//*out_slice = NULL;
	out_slice_without_emulation_bytes = NULL;
	//gf_bs_get_content(bs_rw, out_slice, out_slice_length);
	gf_bs_get_content(bs_rw, &out_slice_without_emulation_bytes, out_slice_length);
	*out_slice = malloc(sizeof(char)*(*out_slice_length));
	//printf("in_slice_length: %d\nout_slice_length: %d\n", in_slice_length, *out_slice_length);	
	avc_add_emulation_bytes(out_slice_without_emulation_bytes, *out_slice, *out_slice_length);
	free(in_slice_without_emulation_bytes);
	free(out_slice_without_emulation_bytes);
	//printf("output buffer size: %d\n", *out_slice_length);
} 

void parse_and_print_PPS(char *buffer, u32 nal_length, HEVCState* hevc, int *tile_num)
{	
	int i = gf_media_hevc_read_pps(buffer, nal_length, hevc);
 	printf("pps id:\t%d\n", i);
    	printf("column_width:\t%u\n",(*hevc).pps[i].column_width[i]);
     	printf("uniform_spacing_flag:\t%d\n",(*hevc).pps[i].uniform_spacing_flag);
     	printf("dependent_slice_segments_enabled_flag:\t%d\n",(*hevc).pps[i].dependent_slice_segments_enabled_flag);
	printf("num_tile_columns:\t%u\n",(*hevc).pps[i].num_tile_columns);
	printf("num_tile_rows:\t%u\n",(*hevc).pps[i].num_tile_rows);
	printf("num_profile_tier_level:\t%hhu\n",(*hevc).vps[i].num_profile_tier_level);
	printf("height:\t%u\n",(*hevc).sps[i].height);
	printf("slice_segment_header_extension_present_flag:\t%d\n",(*hevc).pps[i].slice_segment_header_extension_present_flag);
	*tile_num = (*hevc).pps[i].num_tile_columns * (*hevc).pps[i].num_tile_rows;
}


