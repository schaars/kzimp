all: pipe_test pipe_signal_test pipe_splice_test pipe_select

GCC:=gcc
FLAGS:=-g3 -ggdb -Wall -Werror

%.o: %.c
	$(GCC) $(FLAGS) -o $@ -c $<

pipe_test: pipe_test.o
	$(GCC) $(FLAGS) -o $@ $^

pipe_signal_test: pipe_signal_test.o
	$(GCC) $(FLAGS) -o $@ $^

pipe_splice_test: pipe_splice_test.o
	$(GCC) $(FLAGS) -o $@ $^
	
pipe_select: pipe_select.o
	$(GCC) $(FLAGS) -o $@ $^
	
	
clean:
	-rm *.o
	-rm *~

clobber:
	-rm *.o
	-rm *~
	-rm pipe_test pipe_signal_test pipe_splice_test pipe_select