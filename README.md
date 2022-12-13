# cxl-addrtest
Out of tree CXL address translation

cxl/addrtest: Out of tree test for CXL address translations

Intends to mimic the address calculations found in the CXL kernel
driver. Known differences, noted in the C file, relate to the
creation of masks.
The initial test cases include:
- The 2 examples from the CXL Spec Decode Logic section.
- The 1 example from the CXL Driver Writers Guide translation section
- The 33 examples from the CXL test suite execution.

Add new 'test cases' to addrx_data.json:
Test cases look like this:
{ "dpa": "0x0400", "eig": "2", "eiw": "3", "pos": "5", "hpa": "0x3400" }

DPA & HPA fields are OFFSETS.

./addrs.sh calls C program: "addrx" with each test case.

C program "addrx" verifies DPA->HPA and HPA->DPA translations.
