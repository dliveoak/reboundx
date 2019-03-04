/**
 * @file    steppers.c
 * @brief   Wrappers to run REBOUND steps with different integrators
 * @author  Dan Tamayo, Hanno Rein <tamayo.daniel@gmail.com>
 *
 * @section     LICENSE
 * Copyright (c) 2015 Dan Tamayo, Hanno Rein
 *
 * This file is part of reboundx.
 *
 * reboundx is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * reboundx is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with rebound.  If not, see <http://www.gnu.org/licenses/>.
 *
 * The section after the dollar signs gets built into the documentation by a script.  All lines must start with space * space like below.
 * Tables always must be preceded and followed by a blank line.  See http://docutils.sourceforge.net/docs/user/rst/quickstart.html for a primer on rst.
 * $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
 *
 * $General Relativity$       // Effect category (must be the first non-blank line after dollar signs and between dollar signs to be detected by script).
 *
 * ======================= ===============================================
 * Authors                 P. Shi, D. Tamayo, H. Rein
 * Implementation Paper    *In progress*
 * Based on                `Anderson et al. 1975 <http://labs.adsabs.harvard.edu/adsabs/abs/1975ApJ...200..221A/>`_.
 * C Example               :ref:`c_example_gr`
 * Python Example          `GeneralRelativity.ipynb <https://github.com/dtamayo/reboundx/blob/master/ipython_examples/GeneralRelativity.ipynb>`_.
 * ======================= ===============================================
 *
 * This assumes that the masses are dominated by a single central body, and should be good enough for most applications with planets orbiting single stars.
 * It ignores terms that are smaller by of order the mass ratio with the central body.
 * It gets both the mean motion and precession correct, and will be significantly faster than :ref:`gr_full`, particularly with several bodies.
 * Adding this effect to several bodies is NOT equivalent to using gr_full.
 *
 * **Effect Parameters**
 *
 * ============================ =========== ==================================================================
 * Field (C type)               Required    Description
 * ============================ =========== ==================================================================
 * c (double)                   Yes         Speed of light in the units used for the simulation.
 * ============================ =========== ==================================================================
 *
 * **Particle Parameters**
 *
 * If no particles have gr_source set, effect will assume the particle at index 0 in the particles array is the source.
 *
 * ============================ =========== ==================================================================
 * Field (C type)               Required    Description
 * ============================ =========== ==================================================================
 * gr_source (int)              No          Flag identifying the particle as the source of perturbations.
 * ============================ =========== ==================================================================
 *
 */

#include <math.h>
#include "rebound.h"
#include "reboundx.h"

// will do IAS with gravity + any additional_forces

void rebx_ias15_step(struct reb_simulation* const sim, struct rebx_operator* const operator, const double dt){
    const double old_t = sim->t;
    const double t_needed = old_t + dt;
    const double old_dt = sim->dt;
    sim->gravity_ignore_terms = 0;
    reb_integrator_ias15_reset(sim);
    
    sim->dt = 0.0001*dt; // start with a small timestep.
    
    while(sim->t < t_needed && fabs(sim->dt/old_dt)>1e-14 ){
        reb_update_acceleration(sim);
        reb_integrator_ias15_part2(sim);
        if (sim->t+sim->dt > t_needed){
            sim->dt = t_needed-sim->t;
        }
    }
    sim->t = old_t;
    sim->dt = old_dt; // reset in case this is part of a chain of steps
}

void rebx_kepler_step(struct reb_simulation* const sim, struct rebx_operator* const operator, const double dt){
    reb_integrator_whfast_reset(sim);
    reb_integrator_whfast_init(sim);
    reb_integrator_whfast_from_inertial(sim);
    reb_whfast_kepler_step(sim, dt);
    reb_whfast_com_step(sim, dt);
    reb_integrator_whfast_to_inertial(sim);
}

void rebx_jump_step(struct reb_simulation* const sim, struct rebx_operator* const operator, const double dt){
    reb_integrator_whfast_init(sim);
    reb_integrator_whfast_from_inertial(sim);
    reb_whfast_jump_step(sim, dt);
    reb_integrator_whfast_to_inertial(sim);
}

void rebx_interaction_step(struct reb_simulation* const sim, struct rebx_operator* const operator, const double dt){
    reb_integrator_whfast_init(sim);
    reb_integrator_whfast_from_inertial(sim);
    reb_update_acceleration(sim);
    reb_whfast_interaction_step(sim, dt);
    reb_integrator_whfast_to_inertial(sim);
}


