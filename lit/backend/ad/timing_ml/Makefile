all: compile run cleanup

FILES=preopt.ml opt.ml test.ml


test.out:
	ocamlc -o test.out ${FILES}
test_opt.out:
	ocamlopt -o test_opt.out ${FILES}
test_opt3.out:
	ocamlopt -O3 -o test_opt3.out ${FILES}
compile: test.out test_opt.out test_opt3.out

run: test.out test_opt.out test_opt3.out
	./test.out
	./test_opt.out
	./test_opt3.out

cleanup:
	rm -f *.cmo *.cmi *.out *.o *.cmx



