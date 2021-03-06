
#include "block.h"

struct Block * each_block(struct Block * block);
int each_shape(struct Block * block, uint32_t shape_start_id, uint32_t shape_end_id);
int each_part(struct Block * block, uint32_t part_start_id, uint32_t part_end_id);

// http://www.gamedev.net/topic/564749-2d-polygon-winding-order/

static char direction[100] = "";

struct Block * each_block(struct Block * block) {
	foreach_shape(block, &each_shape);
	return block;
}

int each_shape(struct Block * block, uint32_t shape_start_id, uint32_t shape_end_id) {
	foreach_part(block, shape_start_id, shape_end_id, &each_part);
	return 0;
}

int each_part(struct Block * block, uint32_t part_start_id, uint32_t part_end_id) {
	double area = 0;
	
	int i;
	for (i = part_start_id ; i < part_end_id-1 ; i++) {
		area += (get_x(block, i) * get_y(block, i+1) - get_x(block, i+1) * get_y(block, i));
	}
	
	if ((strcmp(direction, "cw")==0 && area < 0) || (strcmp(direction, "ccw")==0 && area > 0) || strcmp(direction, "reverse")==0) {
		void * temp = malloc(block->row_bsize);
		memset(temp, 0, block->row_bsize);
		int j = part_end_id-1;
		for (i = part_start_id ; i < part_end_id ; i++,j--) {
			if (i == j) break;
			if (i > j) break;
			memmove(temp, get_row(block, i), block->row_bsize);
			memmove(get_row(block, i), get_row(block, j), block->row_bsize);
			memmove(get_row(block, j), temp, block->row_bsize);
		}
		free(temp);
	}
	return 0;
}

int main(int argc, char ** argv) {
	
	if (stdout_is_piped()) // other wise you don't see the seg fault
		setup_segfault_handling(argv);
	
	assert_stdin_is_piped();
	assert_stdout_is_piped();
	
	struct Params * params = NULL;
	params = add_string_param(params, "direction", 'd', direction, 1);
	eval_params(params, argc, argv);
	
	if (strcmp(direction, "cw")!=0 && 
			strcmp(direction, "ccw")!=0 && 
			strcmp(direction, "reverse")!=0) {
		fprintf(stderr, "%s: ERROR: param 'direction' needs to be 'cw' or 'ccw' or 'reverse' ABORTING\n", argv[0]);
		abort();
	}
	
	foreach_block(stdin, stdout, 1, &each_block);
}
















