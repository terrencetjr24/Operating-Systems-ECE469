default:
	cd makeprocs; make
	cd S2Gen; make
	cd CoGen; make
	cd react1; make
	cd react2; make
	cd react3; make

clean:
	cd makeprocs; make clean
	cd S2Gen; make clean
	cd CoGen; make clean
	cd react1; make clean
	cd react2; make clean
	cd react3; make clean

run:
	cd ../../bin; dlxsim -x os.dlx.obj -a -u makeprocs.dlx.obj 8 8; ee469_fixterminal
