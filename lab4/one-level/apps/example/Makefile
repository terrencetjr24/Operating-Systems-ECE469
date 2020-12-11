default:
	cd makeprocs; make
	cd hello_world; make
	cd grow_call_stack; make
	cd invalid_mem_access; make
	cd out_of_bound_vm_access; make
	cd simultaneous_process; make

clean:
	cd makeprocs; make clean
	cd hello_world; make clean
	cd grow_call_stack; make clean
	cd invalid_mem_access; make clean
	cd out_of_bound_vm_access; make clean
	cd simultaneous_process; make clean

run:
	cd ../../bin; dlxsim -x os.dlx.obj -a -u makeprocs.dlx.obj; ee469_fixterminal
