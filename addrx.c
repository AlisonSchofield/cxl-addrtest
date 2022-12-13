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


/*
 * Intent is to be as close to possible as the kernel calculation.
 * At the moment, this differs due to the masking functions.
 */

int main(int argc, char *argv[])
{
	unsigned long long dpa_offset = strtoull(argv[1], NULL, 0);
	unsigned long long hpa_offset = strtoull(argv[2], NULL, 0);
	unsigned long long mask_upper, mask_lower, dpa_upper_mask;
	unsigned long long bits_upper, bits_lower;
	unsigned long long test_offset;
	int eig = atoi(argv[3]);
	int eiw = atoi(argv[4]);
	int pos = atoi(argv[5]);

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
		test_offset = (hpa_offset & mask_upper) >> eiw;
	} else {
		mask_upper = MY_GENMASK_ULL(51, eig + eiw);
		bits_upper = (hpa_offset & mask_upper) >> eig + eiw;
		bits_upper = bits_upper / 3;
		test_offset = bits_upper << eig + 8;	
	}

	/* Lower bits don't change */
	mask_lower = (1 << (eig + 8)) -1;
	bits_lower = hpa_offset & mask_lower;
	test_offset += bits_lower;

	if (test_offset != dpa_offset)
		exit(EXIT_FAILURE);

	/*
	 * Translate DPA to HPA
	 * Reverse the above decode logic to get back to an HPA
	 * Insert the 'pos' to construct the HPA.
	 */
	if (eiw < 8) {
		test_offset = (dpa_offset & MY_GENMASK_ULL(51, eig + 8)) << eiw;
		test_offset |= pos << (eig + 8);
	} else {
		mask_upper = MY_GENMASK_ULL(51, eig + 8);
		bits_upper = (dpa_offset & mask_upper) >> eig + 8;
		bits_upper = bits_upper * 3;
		test_offset = bits_upper << eig + eiw;
	}

	/* Lower bits don't change */
	mask_lower = (1 << (eig + 8)) -1;
	bits_lower = hpa_offset & mask_lower;
	test_offset += bits_lower;

	if (test_offset != hpa_offset)
		exit(EXIT_FAILURE);

	exit(EXIT_SUCCESS);
}
