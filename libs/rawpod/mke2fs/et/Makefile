OBJS = com_err.o com_right.o error_message.o et_name.o init_et.o
all: $(OBJS)
$(OBJS): %.o: %.cc
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -c -o $@ $<
