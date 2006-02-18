#!/usr/bin/python

import sys

###### Our generic parse exception class - used to teach davidc__ exceptions!
class ParseError:
	error=""
	def __init__(self, error_str):
		self.error = error_str

	def __str__(self):
		return self.error

##### Our global SFR type
class SFR:
	start_addr = 0
	elem_size = 0
	elem_count = 0
	name = "UNASSIGNED"
	elem_init = []

	def __init__(self, sfr_addr, sfr_name, size=4, count=1, init=[0]):
		self.start_addr = sfr_addr
		self.name = sfr_name
		self.elem_size = size
		self.elem_count = count
		self.elem_init = init

	def __str__ (self):
		return "%#x %s %d %d %s" % (self.start_addr, self.name,
			self.elem_size, self.elem_count, self.elem_init) 

###### define our globals
global sfr_list
sfr_list = []
global proc_type
proc_type = "UNKNOWN"


################# CODE GENERATORS ####################

################# PARSING CODE #######################

###### proc
def handle_proc(line):
	global proc_type
	proc_type = line[1]

###### sfr

def create_default_sfr(base, name, args):
	# We need to parse the args
	s = SFR(base,name)
	sfr_list.append(s)

def handle_sfr(line):
	start_addr_text = line[1]

	# Remove colon separators
	start_addr_text = start_addr_text.replace(":","")

	# Add an 0x
	if (start_addr_text[:1].lower() != "0x"):
		start_addr_text = "0x" + start_addr_text
	
	start_addr = int(start_addr_text,16)

	name = line[2]

	if (len(line) > 3):
		type = line[3].upper()
	else:
		type = "DEFAULT"

	# Now - decide based upon the type, what kind of SFR to instantiate
	if (type == "DEFAULT"):
		create_default_sfr(start_addr, name, line[4:])
	else:
		raise ParseError ("Unrecognized SFR type %s" % type)


###### line parse
def handle_line(line):
	kwd = line[0].upper();

	if (kwd == "PROC"):
		handle_proc(line)
	elif (kwd == "SFR"):
		handle_sfr(line)


###### main

# Make sure the args are sane
if len(sys.argv) != 2:
	print "Error - usage is " + sys.argv[0] + " filename\n"

file_name = sys.argv[1]

# calculate the file prefix we'll write to
file_per_ind = file_name.rfind(".")

if file_per_ind == -1:
	print "Error - could not understand filename"
	sys.exit(0)

file_prefix = file_name[:file_per_ind]


# open our file
f = open(file_name)

line_num = 0

# iterate through all the lines
line = f.readline()
while (line != ""):
	line_num = line_num + 1

	comment_end = line.find("#")

	if (comment_end != -1):
		line = line[0:comment_end]

	# strip off all excess whitespace
	line = line.strip();

	# split the line
	line_ar = line.split();
	
	try:
		if (len(line_ar)!=0):
			handle_line(line_ar)
	except ParseError, err:
		print "[%4d] Parse error: " % (line_num) + str(err)


	line = f.readline()

f.close()

print proc_type
print "["
for s in sfr_list:
	print s

print "]"
###### end main


