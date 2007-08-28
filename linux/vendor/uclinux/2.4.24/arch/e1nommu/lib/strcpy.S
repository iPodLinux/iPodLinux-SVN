/*
 * Derived from the Hyperstone's library source code.
 * Modefied src in order to apply the -mgnu-param compiler option.
 * Copyright (C) 2002-2003 GDT, Yannis Mitsos <yannis.mitsos@gdt.gr>
 *                              George Thanos <george.thanos@gdt.gr>
 */
	    .text
	    .align   2
	    .global  _strcpy
;ENTRY(_strcpy)
_strcpy:
	FRAME	L9,L2

#*********************************
# Perform byte copy if both not
# aligned on a word
#*********************************
	CMPBI	L1,3		# check word alignment
	BNZ	str_except
	CMPBI	L0,3		# check word alignment
	BNZ	str_except


	MOV	L4,L0		# load s2 into L4
	MOV	L0,L1		# load s1 into L0
	BR	start

loop:
	BE	label_3	
	STD.P	L0,L7		# post inc
	
start:
	LDD.P	L4,L5		# post inc
	LDD.P	L4,L7		# post inc
	CMPBI	L5,ANYBZ	# test for null
	BE	label_1		
	CMPBI	L6,ANYBZ	# test for null
	BE	label_2		
	STD.P	L0,L5		# post inc
	CMPBI	L7,ANYBZ	# test for null
	DBNE	loop		
	CMPBI	L8,ANYBZ	# test for null
	DBR	label_4
	MOV 	L8,L7		# move to L8

label_1:			# move L5 to L8
	DBR	label_4
	MOV	L8,L5

label_2:			# store L5, move L6
	STW.P	L0,L5		# post inc
	DBR	label_4
	MOV	L8,L6

label_3:			# store L7
	FETCH	16
	STW.P	L0,L7		# post inc

label_4:			# locate null
	CMPBI	L8,0xFF000000	# 1st byte
	BZ	ret_1
	CMPBI	L8,0xFF0000	# 2nd byte
	BZ	ret_2
	CMPBI	L8,0xFF00	# 3rd byte
	BNZ	ret_3

	SHRI	L8,16		# store 2-bytes
	STHU.N	L0,L8,2		# half word, next address

ret_1:
	STBU.D	L0,0,0		# store null
	RET	PC,L2

ret_2:
	SHRI	L8,16		# store 2 bytes, includes null
	STHU.D	L0,L8,0
	RET	PC,L2

ret_3:  
	STW.R	L0,L8		# store entire word, includes nullstr1_except:
	RET	PC,L2

#******************************
# Byte strcpy
#******************************
str_except:
	MOV	L5,L1		# Save the pointer
L_4:
	LDBU.N	L0,L4,1		# Copy a byte, exit if NULL
	STBU.N	L5,L4,1
	CMPI	L4,0
	BNE	L_4
	RET	PC,L2
	.END
