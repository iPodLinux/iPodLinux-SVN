OBJS = alloc.o alloc_sb.o alloc_stats.o alloc_tables.o badblocks.o \
       bb_compat.o bb_inode.o bitmaps.o bitops.o block.o bmap.o \
       brel_ma.o check_desc.o closefs.o cmp_bitmaps.o dblist.o \
       dblist_dir.o dir_iterate.o dirblock.o dirhash.o dupfs.o \
       expanddir.o ext2_err.o fileio.o freefs.o gen_bitmap.o \
       get_pathname.o getsectsize.o icount.o initialize.o inline.o \
       inode.o irel_ma.o ismounted.o link.o lookup.o mkdir.o \
       mkjournal.o namei.o native.o newdir.o openfs.o read_bb.o \
       read_bb_file.o rs_bitmap.o rw_bitmaps.o swapfs.o unix_io.o \
       unlink.o valid_blk.o version.o write_bb_file.o 

all: $(OBJS)
$(OBJS): %.o: %.cc
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -c -o $@ $<
