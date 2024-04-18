// SPDX-License-Identifier: GPL-2.0
/*
 * Tests CXL DPA <--> HPA address translations
 *
 * Tests the offset calculations
 *
 * COMPILE  ==>  cc -o addrx addrx.c
 *
 */
#define _GNU_SOURCE
#include <errno.h>
#include <sys/syscall.h>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <limits.h>

typedef uint64_t u64;

/*
 * "MY_" macros are trying (poorly?) to mimic kernel macros.
 * Wondering what is available in user space.
 */
#define MY_BIT(nr)  (1 << nr)
#define MY_BITS_PER_LONG_LONG 64

#define MY_GENMASK_ULL(h, l) \
	(((~(0)) - ((1) << (l)) + 1) & \
	 (~(0) >> (MY_BITS_PER_LONG_LONG - 1 - (h))))


typedef uint64_t u64;

int count_bits_set(u64 value) {
    int count = 0;
    while (value) {
        count += value & 1;
        value >>= 1;
    }
    return count;
}

u64 __restore_xor_pos(u64 hpa, u64 map)
{
    int n = 0;

    /* Count the number of bits set in (hpa & cximsd->xormaps[i]) */
    int bits_set = count_bits_set(hpa & map);

    /* XOR of all set bits */
    n = (bits_set & 1);

    /* Find the lowest set bit in the map */
    int lowest_bit_pos = 0;
    while ((map & (1ULL << lowest_bit_pos)) == 0) {
        lowest_bit_pos++;
    }

    /* Set the bit at hpa[lowest_bit_pos] to n */
    if (n) {
        hpa |= (1ULL << lowest_bit_pos);
    } else {
        hpa &= ~(1ULL << lowest_bit_pos);
    }

    return hpa;
}

u64 restore_xor_pos(u64 hpa_offset, uint8_t eiw)
{
	u64 temp_a, temp_b, temp_c;

	switch (eiw) {
	case 0: /* 1-way */
	case 8: /* 3-way */
		return hpa_offset;

	case 1:
		/* 2-way */
		return __restore_xor_pos(hpa_offset, 0x404100);

	case 2: /* 4-way */
		temp_a = __restore_xor_pos(hpa_offset, 0x404100);
		return __restore_xor_pos(temp_a, 0x808200);

	case 3: /* 8-way */
		temp_a = __restore_xor_pos(hpa_offset, 0x404100);
		temp_b = __restore_xor_pos(temp_a, 0x808200);
		return __restore_xor_pos(temp_b, 0x1010400);

	case 4: /* 16-way */
		temp_a = __restore_xor_pos(hpa_offset, 0x404100);
		temp_b = __restore_xor_pos(temp_a, 0x808200);
		temp_c = __restore_xor_pos(temp_b, 0x1010400);
		return __restore_xor_pos(temp_b, 0x800);

	case 9: /* 6-way */
		return __restore_xor_pos(hpa_offset, 0x2020900);

	case 10: /* 12-way */
		temp_a = __restore_xor_pos(hpa_offset, 0x2020900);
		return __restore_xor_pos(temp_a, 0x4041200);

	default:
		return ULLONG_MAX;
	}

	return ULLONG_MAX;
}

u64 to_dpa(unsigned long long hpa_offset, uint16_t eig, uint8_t eiw)
{
	u64 mask_upper, mask_lower, dpa_upper_mask;
	u64 bits_upper, bits_lower;
	u64 dpa_offset;
	
	/*
	 * Translate HPA to DPA
	 *
	 * Follow the HPA->DPA decode login defined in CXL Spec 3.0
	 * Section 8.2.4.19.13  Implementation Note: Device Decode Logic
	 *
	 * Remove the 'pos' to construct a DPA.
	 */
	if (eiw < 8) {
		mask_upper = MY_GENMASK_ULL(51, eig + 8 + eiw);
		dpa_offset = (hpa_offset & mask_upper) >> eiw;
	} else {
		mask_upper = MY_GENMASK_ULL(51, eig + eiw);
		bits_upper = (hpa_offset & mask_upper) >> eig + eiw;
		bits_upper = bits_upper / 3;
		dpa_offset = bits_upper << eig + 8;
	}

	/* Lower bits don't change */
	mask_lower = (1 << (eig + 8)) -1;
	bits_lower = hpa_offset & mask_lower;
	dpa_offset += bits_lower;

	return dpa_offset;
}

void to_hpa(u64 dpa_offset, uint16_t eig, uint8_t eiw, int pos)
{
	u64 mask_upper, mask_lower, dpa_upper_mask;
	u64 bits_upper, bits_lower;
	u64 hpa_offset, test_offset;
	u64 hpa_offset_modulo, hpa_offset_xor;

	/*
	 * Translate DPA to HPA
	 * Reverse the above decode logic to get back to an HPA
	 * Insert the 'pos' to construct the HPA.
	 */
	if (eiw < 8) {
		hpa_offset = (dpa_offset & MY_GENMASK_ULL(51, eig + 8)) << eiw;
		hpa_offset |= pos << (eig + 8);
	} else {
		mask_upper = MY_GENMASK_ULL(51, eig + 8);
		bits_upper = (dpa_offset & mask_upper) >> (eig + 8);
		bits_upper = bits_upper * 3;
		hpa_offset = ((bits_upper << (eiw - 8)) + pos) << (eig + 8);
	}

	/* Lower bits don't change */
	mask_lower = (1 << (eig + 8)) -1;
	bits_lower = dpa_offset & mask_lower;
	hpa_offset += bits_lower;

	/* Now go back to the DPA offset for a sanity check */
	test_offset = to_dpa(hpa_offset, eig, eiw);

	if (test_offset != dpa_offset) {
		printf("FAIL: test_offset=0x%llx dpa_offset=0x%llx\n",
			test_offset, dpa_offset);
		printf("FAIL: ");
	} else {
		printf("Pass: ");
	}

	hpa_offset_modulo = hpa_offset;
	hpa_offset_xor = restore_xor_pos(hpa_offset, eiw);

	printf("dpa_offset=0x%llx eig=%u eiw=%u pos=%d hpa_offset_modulo=0x%llx hpa_offset_xor=0x%llx\n",
	       dpa_offset, eig, eiw, pos, hpa_offset_modulo, hpa_offset_xor);
}

int eiw_to_ways(uint8_t eiw, unsigned int *ways)
{
        switch (eiw) {
        case 0 ... 4:
                *ways = 1 << eiw;
                break;
        case 8 ... 10:
                *ways = 3 << (eiw - 8);
                break;
        default:
                return -EINVAL;
        }

        return 0;
}

int main(int argc, char *argv[])
{
	u64 dpa_offset = strtoull(argv[1], NULL, 0);
	int i, j, k, ways; 
	
	uint8_t eiws[] = {0, 1, 2, 3, 4, 8, 9, 10};
	int nr_eiws = 8;
	uint16_t eigs[] = {0, 1, 2, 3, 4, 5, 6};
	int nr_eigs = 7;

	if ((dpa_offset & 0x07) != 0) {
		printf("Invalid dpa_offset - must be 64b aligned\n");
		exit(EXIT_FAILURE);
	}

	for (i = 0; i < nr_eigs; i++)
		for (j = 1; j < nr_eiws; j++) {
			eiw_to_ways(eiws[j], &ways);
			for (k = 0; k < ways; k++) 
				to_hpa(dpa_offset, eigs[i], eiws[j], k);
		}
}
