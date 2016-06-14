default: teamcenter-server

run:
	./teamcenter-server -c cppcms.js

teamcenter-server: teamcenter-server.cpp
	c++ -Wall teamcenter-server.cpp -o teamcenter-server -Lcppcms/lib -Icppcms/include -lcppcms -lbooster -lz

clean:
	rm -fr teamcenter-server *.exe *.so cppcms_rundir

