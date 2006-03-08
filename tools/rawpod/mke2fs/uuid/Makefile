OBJS = clear.o compare.o copy.o gen_uuid.o isnull.o pack.o parse.o unpack.o unparse.o
all: $(OBJS)
$(OBJS): %.o: %.cc
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -c -o $@ $<
