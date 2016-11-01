all: sp-prob-3b.c sp-prob-3b-test.c
	gcc sp-prob-3b.c -o 3b
	gcc sp-prob-3b-test.c -o a.out

test: 3b a.out
	echo "NTUSP2016" > /tmp/infile
	rm -f /tmp/outfile
clean:
	rm -f 3b
	-rm -i a.out
	rm -f /tmp/infile /tmp/outfile

