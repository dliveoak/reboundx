ifndef REB_DIR
	
ifneq ($(wildcard ../../rebound/.*),) # Check for REBOUND in default location
REB_DIR=../../rebound
endif

ifneq ($(wildcard ../../../rebound/.*),) # Check for REBOUNDx being inside the REBOUND directory
REB_DIR=../../
endif

endif

ifndef REB_DIR # REBOUND is not in default location and REB_DIR is not set
    $(error REBOUNDx not in the same directory as REBOUND.  To use a custom location, you Must set the REB_DIR environment variable for the path to your rebound directory, e.g., export REB_DIR=/Users/dtamayo/rebound.  See reboundx.readthedocs.org)
endif

include $(REB_DIR)/src/Makefile.defs
OPT+= -fPIC -DLIBREBOUNDX

ifndef REBXGITHASH
	REBXGITHASH = $(shell git rev-parse HEAD || echo '0000000000gitnotfound0000000000000000000')
	PREDEF+= -DREBXGITHASH=$(REBXGITHASH)
endif

SOURCES=central_force.c core.c exponential_migration.c gas_damping_timescale.c gas_dynamical_friction.c gr.c gravitational_harmonics.c gr_full.c gr_potential.c inner_disk_edge.c input.c integrate_force.c integrator_euler.c integrator_implicit_midpoint.c integrator_rk2.c integrator_rk4.c interpolation.c lense_thirring.c linkedlist.c modify_mass.c modify_orbits_direct.c modify_orbits_forces.c output.c radiation_forces.c rebxtools.c steppers.c stochastic_forces.c tides_constant_time_lag.c tides_dynamical.c tides_spin.c track_min_distance.c type_I_migration.c yarkovsky_effect.c 

OBJECTS=$(SOURCES:.c=.o)
HEADERS=rebxtools.h reboundx.h linkedlist.h

all: $(SOURCES) librebound.so libreboundx.so
	
%.o: %.c $(HEADERS)
	@echo "Compiling source file $< ..."
	$(CC) -c $(OPT) $(PREDEF) -I$(REB_DIR)/src -o $@ $<

librebound.so:
	@echo "Compiling shared library librebound.so ..."
	$(MAKE) -C $(REB_DIR)/src/
	@echo "Creating link for shared library librebound.so ..."
	@-rm -f librebound.so
	@ln -s $(REB_DIR)/src/librebound.so .

libreboundx.so: $(OBJECTS)
	@echo ""        
	@echo "Linking shared library $@ ..."
	$(CC) $(OPT) -shared $(OBJECTS) $(LIB) -lrebound -L$(REB_DIR)/src -o $@ 
	@echo ""        
	@echo "The shared library $@ has been created successfully."
	
clean:
	@echo "Cleaning up shared library librebound.so ..."
	@-rm -f librebound.so
	$(MAKE) -C $(REB_DIR)/src/ clean
	@echo "Cleaning up shared library libreboundx.so ..."
	@-rm -f libreboundx.so
	@-rm -f *.o
	
