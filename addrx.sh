#!/bin/bash
# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2022 Intel Corporation. All rights reserved.

# try to use to_entries
# eval "$(echo "$line" | jq '.[] | to_entries | .key + "=" + (.value | @sh)')"

translate_test()
{

# try to use to_entries
# eval "$(echo "$line" | jq '.[] | to_entries | .key + "=" + (.value | @sh)')"

	dpa=$(echo "$line" | jq -r ".dpa")
	hpa=$(echo "$line" | jq -r ".hpa")
	eig=$(echo "$line" | jq -r ".eig")
	eiw=$(echo "$line" | jq -r ".eiw")
	pos=$(echo "$line" | jq -r ".pos")

	./addrx $dpa $hpa $eig $eiw $pos
	rc=$?
	if [ $rc -eq 0 ]; then
		echo "Pass: $line"
	else
		echo "Fail: $line"
	fi
}

input="addrx_data.json"
while IFS= read -r line
do
	translate_test
done < "$input"

