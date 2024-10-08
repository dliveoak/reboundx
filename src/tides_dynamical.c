/**
 * @file    tides_dynamical.c
 * @brief   Update body's orbital and modal evolution due to the presence of dynamical tides.
 * @author  Donald J. Liveoak <donaldliveoak1@gmail.com>
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
 * $Tides$       // Effect category (must be the first non-blank line after dollar signs and between dollar signs to be detected by script).
 *
 * ======================= ===============================================
 * Authors                 D. Liveoak & S. Millholland
 * Implementation Paper    
 * Based on                `Vick et al. 2019 <https://academic.oup.com/mnras/article/484/4/5645/5306464/>`_. 
 * C Example               
 * Python Example          
 * ======================= ===============================================
 * 
 * This updates body's orbital and modal evolution due to the presence of dynamical tides.
 * Particles are modeled by a gamma=4/3 polytrope, and the f-mode is evolved at each pericentre passage.
 * The dissipation of orbital energy due to dynamical tides is modeled as an angular momentum-conserving kick at periapse.
 * When mode energy grows to exceed `td_E_max`, it is non-linearly dissipated in one orbital period to `td_E_resid`.
 * To isolate the effects of chaotic model evolution, one can set `dP_hat_crit` to disable dynamical tides whenever chaos is unlikely (see Vick et al. (2019))
 * 
 * 
 * **Particle Parameters**
 *
 * One can pick and choose which particles have which parameters set.  
 * For each particle, any unset parameter is replaced by its default value.
 * Particles with index 0 will not experience dynamical tides.
 *
 * ============================ =========== ==================================================================
 * Field (C type)               Required    Description
 * ============================ =========== ==================================================================
 * particles[i].r (float)       Yes         Radius of ith particle
 * td_dP_crit (double)          No          Critical mode phase change to evolve tides (see Vick et al. (2019), default 0.01)
 * td_c_imag (double)           No          Imaginary part of initial mode amplitude (default: 0.0)
 * td_c_real (double)           No          Real part of initial mode amplitude (default: 0.0)
 * td_EB0 (double)              No          Intitial orbital energy (default: -GMm / (2a_0))
 * td_E_max (double)            No          Maximum mode energy (default: 0.1 * E_bind)
 * td_E_resid (double)          No          Residual mode energy after non-linear dissipation (default: 1E-3 * E_bind)
 * ============================ =========== ==================================================================
 * 
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "rebound.h"
#include "reboundx.h"
#include "rebxtools.h"

struct rebx_tides_dynamical_params rebx_calculate_tides_dynamical_params(struct reb_simulation* const sim, struct reb_particle* p, struct reb_particle* primary)
{

    struct rebx_extras* const rebx = sim->extras;
    const double EulerConstant = 2.718281828459;

    // Calculate orbital elements
    struct reb_orbit o = reb_orbit_from_particle(sim->G, *p, *primary);
    double e = o.e;
    double a = o.a;
    double n = o.n;
    double P = o.P;

    // Calculate some useful distances
    double R = p->r; // radius of planet
    double R_tide = R * pow(primary->m / p->m, 1 / 3); // tidal radius
    double R_p = a * (1 - e); // pericenter distance
    double eta = R_p / R_tide; // pericenter distance in units of tidal radius

    // Timescales/frequencies
    double Omega_peri = pow(sim->G * (p->m + primary->m) / (R_p * R_p * R_p), 0.5); // pericenter frequency
    double time_unit = pow(sim->G * p->m / (R * R * R), 0.5); // default units for mode parameters

    // Calculate pseudo-synchronous orbital frequency
    double f2 = 1 + (15 / 2) * pow(e, 2) + (45 / 8) * pow(e, 4) + (5 / 16) * pow(e, 6);
    double f5 = 1 + 3 * pow(e, 2) + (3 / 8) * pow(e, 4);
    double Omega_s = n * f2 / (pow(1 - e * e, 1.5) * f5);

    // Calculate f-mode parameters, gamma=2 polytrope (see Vick et al. (2019))
    double omega = (1.22 - Omega_s / time_unit) * time_unit;
    double sigma = (1.22 + Omega_s / time_unit) * time_unit;
    double epsilon = 1.22 * time_unit;
    double Q = 0.56; // overlap integral

    // Calculate K_22 and T
    double z = pow(2, 0.5) * sigma / Omega_peri;
    double K_22 = 2 * pow(z, 1.5) * pow(eta, 1.5) * pow(EulerConstant, -2 * z / 3) * (1 - pow(M_PI, 0.5) / (4 * pow(z, 0.5))) / (pow(15, 0.5));
    double T = 2 * M_PI * M_PI * Q * Q * K_22 * K_22 * sigma / epsilon;

    // Calculate change in mode energy, assuming 0 mode amplitude
    double dE_alpha = sim->G * primary->m * primary->m * pow(R, 5) * T / pow(R_p, 6);

    // Calculate dP
    double* EB0 = rebx_get_param(rebx, p->ap, "td_EB0");
    double* c_real = rebx_get_param(rebx, p->ap, "td_c_real"); 
    double* c_imag = rebx_get_param(rebx, p->ap, "td_c_imag");
    double maxE = dE_alpha + 2 * pow(-dE_alpha * (pow(*c_real, 2) + pow(*c_imag, 2)) * *EB0, 0.5);
    double EBk = -sim->G * p->m * primary->m / (2 * a);
    double dP = 1.5 * sigma * P * maxE / (-EBk);

    struct rebx_tides_dynamical_params toReturn;
    toReturn.dP = dP;
    toReturn.dE_alpha = dE_alpha;
    toReturn.sigma = sigma;

    return toReturn;
}

struct rebx_tides_dynamical_mode rebx_calculate_tides_dynamical_mode_evolution(double old_real, double old_imag, double dc_tilde, double P, double sigma)
{
    double new_real = (old_real + dc_tilde) * cos(sigma * P) + old_imag * sin(sigma * P);
    double new_imag = -(old_real + dc_tilde) * sin(sigma * P) + old_imag * cos(sigma * P);

    struct rebx_tides_dynamical_mode mode;
    mode.real = new_real;
    mode.imag = new_imag;

    return mode;
}


void rebx_tides_dynamical(struct reb_simulation* const sim, struct rebx_operator* const operator, const double dt){

   	struct rebx_extras* const rebx = sim->extras;
    struct reb_particle* const source = &sim->particles[0];
    struct reb_particle* const p = &sim->particles[1];

    struct reb_orbit o = reb_orbit_from_particle(sim->G, *p, *source);

    // Set default parameter values
    if (rebx_get_param(rebx, p->ap, "td_EB0") == NULL)
    {
        double EB0 = -sim->G * p->m * source->m / (2 * o.a);
        rebx_set_param_double(rebx, (struct rebx_node**)&p->ap, "td_EB0", EB0);    
    }
    if (rebx_get_param(rebx, p->ap, "td_num_periapse") == NULL)
    {
        rebx_set_param_int(rebx, (struct rebx_node**)&p->ap, "td_num_periapse", 0);    
    }
    if (rebx_get_param(rebx, p->ap, "td_c_real") == NULL)
    {
        rebx_set_param_double(rebx, (struct rebx_node**)&p->ap, "td_c_real", 0);   
    }
    if (rebx_get_param(rebx, p->ap, "td_c_imag") == NULL)
    {
        rebx_set_param_double(rebx, (struct rebx_node**)&p->ap, "td_c_imag", 0);    
    }
    if (rebx_get_param(rebx, p->ap, "td_dP_crit") == NULL)
    {
        rebx_set_param_double(rebx, (struct rebx_node**)&p->ap, "td_dP_crit", 0.01);    
    }
    if (rebx_get_param(rebx, p->ap, "td_E_max") == NULL)
    {
        double E_bind = sim->G * p->m * p->m / p->r;
        rebx_set_param_double(rebx, (struct rebx_node**)&p->ap, "td_E_max", E_bind / 10); 
    }
    if (rebx_get_param(rebx, p->ap, "td_E_resid") == NULL)
    {
        double E_bind = sim->G * p->m * p->m / p->r;
        rebx_set_param_double(rebx, (struct rebx_node**)&p->ap, "td_E_resid", E_bind / 1000);   
    }
    if (rebx_get_param(rebx, p->ap, "td_dP_hat") == NULL)
    {
        rebx_set_param_double(rebx, (struct rebx_node**)&p->ap, "td_dP_hat", 0);
    }
    if (rebx_get_param(rebx, p->ap, "td_migrate") == NULL)
    {
        rebx_set_param_int(rebx, (struct rebx_node**)&p->ap, "td_migrate", 1);
    }

    if (rebx_get_param(rebx, p->ap, "td_M_last") != NULL)
    {       
        double* M_last = rebx_get_param(rebx, p->ap, "td_M_last");

        // Periapse detection
        if (o.M < *M_last)
        {
            // Count periapse passages
            int* num_periapse = rebx_get_param(rebx, p->ap, "td_num_periapse");
            rebx_set_param_int(rebx, (struct rebx_node**)&p->ap, "td_num_periapse", *num_periapse + 1);

            double* dP_crit = rebx_get_param(rebx, p->ap, "td_dP_crit");
            struct rebx_tides_dynamical_params dynamical_params = rebx_calculate_tides_dynamical_params(sim, p, source);
            double dP = dynamical_params.dP;
            double dE_alpha = dynamical_params.dE_alpha;
            
            rebx_set_param_double(rebx, (struct rebx_node**)&p->ap, "td_dP_hat", dP);
            rebx_set_param_double(rebx, (struct rebx_node**)&p->ap, "td_dE_last", dE_alpha);

            // If system is in chaotic regime, evolve dynamical tides
            if (dP >= *dP_crit)
            {
                double sigma = dynamical_params.sigma;

                double* EB0 = rebx_get_param(rebx, p->ap, "td_EB0");
                double EBk = -sim->G * p->m * source->m / (2 * o.a);
                double dc_tilde = pow(dE_alpha / -*EB0, 0.5);
                double dE_alpha_tilde = dE_alpha / -*EB0;
                double* c_real = rebx_get_param(rebx, p->ap, "td_c_real"); 
                double* c_imag = rebx_get_param(rebx, p->ap, "td_c_imag");

                // Calculate new orbital energy
                double EB_new = EBk - (-*EB0) * (dE_alpha_tilde + 2 * pow(dE_alpha_tilde, 0.5) * *c_real);
                double E_ratio = EBk / EB_new;

                // If amplitude is sufficiently high, non-linear dissipation
                double* E_max = rebx_get_param(rebx, p->ap, "td_E_max"); 
                double* E_resid = rebx_get_param(rebx, p->ap, "td_E_resid");
                if (-(pow(*c_real, 2) + pow(*c_imag, 2)) * *EB0 >= *E_max)
                {
                    double E_dis_ratio = -*E_resid / *EB0;
                    *c_real = pow(E_dis_ratio / (1 + pow(*c_imag, 2) / pow(*c_real, 2)), 0.5);
                    *c_imag = pow(E_dis_ratio / (1 + pow(*c_real, 2) / pow(*c_imag, 2)), 0.5);
                }

                // Calculate new orbital elements
                double a_prime = o.a * E_ratio;
                double e_prime = pow(1 - (1 / E_ratio) * (1 - o.e * o.e), 0.5);
                double P_prime = o.P * pow(E_ratio, -1.5);

                // Evolve modes
                struct rebx_tides_dynamical_mode new_modes = rebx_calculate_tides_dynamical_mode_evolution(*c_real, *c_imag, dc_tilde, P_prime, sigma);
                rebx_set_param_double(rebx, (struct rebx_node**)&p->ap, "td_c_real", new_modes.real);  
                rebx_set_param_double(rebx, (struct rebx_node**)&p->ap, "td_c_imag", new_modes.imag);  

                // Update positions/velocities
                struct reb_particle new_particle = reb_particle_from_orbit(sim->G, *source, p->m, a_prime, e_prime, o.inc, o.Omega, o.omega, o.f);
                p->x = new_particle.x;
                p->y = new_particle.y;
                p->z = new_particle.z;
                p->vx = new_particle.vx;
                p->vy = new_particle.vy;
                p->vz = new_particle.vz;
            }
        }

    }

    
    rebx_set_param_double(rebx, (struct rebx_node**)&p->ap, "td_M_last", o.M);    
    
}
