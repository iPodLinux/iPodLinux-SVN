OBJS = feature.o hashstr.o iod.o ls.o mntopts.o pe.o pf.o ps.o uuid.o
all: $(OBJS)
$(OBJS): %.o: %.cc
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -c -o $@ $<
