# cxl-addrtest
CXL address translation

cxl/addrtest: Demonstrate CXL address translations

Demonstrates DPA->HPA address calculations defined in the CXL Spec.

Usage: ./addrx dpa_offset

Example: $ ./addrx 0x40000008 > your_output_file

A results file (addrx_sample.out) using dpa 0x40000008 is included in the repo.

All permutations of EIG EIW POS are applied to the dpa_offset to make an hpa_offset,

for each EIG
	for each EIW
		for each POS
			Translate dpa_offset to hpa_offsets (modulo & xor) & check

$ grep Pass your_output_file | wc -l  
357

$ grep FAIL your_ouput_file
0  
