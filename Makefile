LDLIBS=-lhidapi-hidraw -ludev

all: lamp_control

clean:
	rm -f lamp_control *.o