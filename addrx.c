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

/*
 * "MY_" macros are trying (poorly?) to mimic kernel macros.
 * Wondering what is available in user space.
 */
#define MY_BIT(nr)  (1 << nr)
#define MY_BITS_PER_LONG_LONG 64

#define MY_GENMASK_ULL(h, l) \
	(((~(0)) - ((1) << (l)) + 1) & \
	 (~(0) >> (MY_BITS_PER_LONG_LONG - 1 - (h))))

unsigned long long to_dpa(unsigned long long hpa_offset,
			  uint16_t eig, uint8_t eiw)
{
	unsigned long long mask_upper, mask_lower, dpa_upper_mask;
	unsigned long long bits_upper, bits_lower;
	unsigned long long dpa_offset;
	
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

void to_hpa(unsigned long long dpa_offset, uint16_t eig, uint8_t eiw, int pos)
{
	unsigned long long mask_upper, mask_lower, dpa_upper_mask;
	unsigned long long bits_upper, bits_lower;
	unsigned long long hpa_offset, test_offset;
	
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

	printf("dpa_offset=0x%llx eig=%u eiw=%u pos=%d hpa_offset=0x%llx\n",
	       dpa_offset, eig, eiw, pos, hpa_offset);
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
	unsigned long long dpa_offset = strtoull(argv[1], NULL, 0);
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


