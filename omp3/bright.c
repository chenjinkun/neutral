#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "bright.h"
#include "../bright_interface.h"
#include "../../comms.h"
#include "../../shared.h"
#include "../../shared_data.h"
#include "../../params.h"

#ifdef MPI
#include "mpi.h"
#endif

// Performs a solve of dependent variables for particle transport.
void solve_transport_2d(
    const int nx, const int ny, const int global_nx, const int global_ny, 
    const int x_off, const int y_off, const double dt, const int ntotal_particles,
    int* nlocal_particles, const int* neighbours, Particle* particles, 
    const double* density, const double* edgex, const double* edgey, 
    const double* edgedx, const double* edgedy, Particle* particles_out, 
    CrossSection* cs_scatter_table, CrossSection* cs_absorb_table, 
    double* scalar_flux_tally, double* energy_deposition_tally, RNPool* rn_pool)
{
  // Initial idea is to use a kind of queue for handling the particles. Presumably
  // this doesn't have to be a carefully ordered queue but lets see how that goes.

  // This is the known starting number of particles
  int facets = 0;
  int collisions = 0;
  int nparticles = *nlocal_particles;
  int nparticles_sent[NNEIGHBOURS];

  // Communication isn't required for edges
  for(int ii = 0; ii < NNEIGHBOURS; ++ii) {
    nparticles_sent[ii] = 0;
  }

  handle_particles(
      global_nx, global_ny, nx, ny, x_off, y_off, 1, dt, neighbours, density, 
      edgex, edgey, edgedx, edgedy, 
      &facets, &collisions, nparticles_sent, 
      ntotal_particles, nparticles, &nparticles, 
      particles, particles_out, 
      cs_scatter_table, cs_absorb_table, 
      scalar_flux_tally, energy_deposition_tally,
      rn_pool);

#ifdef MPI
  while(1) {
    int nneighbours = 0;
    int nparticles_recv[NNEIGHBOURS];
    MPI_Request recv_req[NNEIGHBOURS];
    MPI_Request send_req[NNEIGHBOURS];
    for(int ii = 0; ii < NNEIGHBOURS; ++ii) {
      // Initialise received particles
      nparticles_recv[ii] = 0;

      // No communication required at edge
      if(neighbours[ii] == EDGE) {
        continue;
      }

      // Check which neighbours are sending some particles
      MPI_Irecv(
          &nparticles_recv[ii], 1, MPI_INT, neighbours[ii],
          TAG_SEND_RECV, MPI_COMM_WORLD, &recv_req[nneighbours]);
      MPI_Isend(
          &nparticles_sent[ii], 1, MPI_INT, neighbours[ii],
          TAG_SEND_RECV, MPI_COMM_WORLD, &send_req[nneighbours++]);
    }

    MPI_Waitall(
        nneighbours, recv_req, MPI_STATUSES_IGNORE);
    nneighbours = 0;

    // Manage all of the received particles
    int nunprocessed_particles = 0;
    const int unprocessed_start = nparticles;
    for(int ii = 0; ii < NNEIGHBOURS; ++ii) {
      if(neighbours[ii] == EDGE) {
        continue;
      }

      // Receive the particles from this neighbour
      for(int jj = 0; jj < nparticles_recv[ii]; ++jj) {
        MPI_Recv(
            &particles[unprocessed_start+nunprocessed_particles], 
            1, particle_type, neighbours[ii], TAG_PARTICLE, 
            MPI_COMM_WORLD, MPI_STATUSES_IGNORE);
        nunprocessed_particles++;
      }

      nparticles_recv[ii] = 0;
      nparticles_sent[ii] = 0;
    }

    nparticles += nunprocessed_particles;
    if(nunprocessed_particles) {
      handle_particles(
          global_nx, global_ny, nx, ny, x_off, y_off, 0, dt, neighbours,
          density, edgex, edgey, edgedx, edgedy, &facets, &collisions, 
          nparticles_sent, ntotal_particles, nunprocessed_particles, &nparticles, 
          &particles[unprocessed_start], particles_out, cs_scatter_table, 
          cs_absorb_table, scalar_flux_tally, energy_deposition_tally, rn_pool);
    }

    // Check if any of the ranks had unprocessed particles
    int particles_to_process;
    MPI_Allreduce(
        &nunprocessed_particles, &particles_to_process, 
        1, MPI_INT, MPI_SUM, MPI_COMM_WORLD);

    // All ranks have reached census
    if(!particles_to_process) {
      break;
    }
  }
#endif

  barrier();

  *nlocal_particles = nparticles;

  printf("facets %d collisions %d\n", facets, collisions);
}

// Handles the current active batch of particles
void handle_particles(
    const int global_nx, const int global_ny, const int nx, const int ny, 
    const int x_off, const int y_off, const int initial, const double dt, 
    const int* neighbours, const double* density, const double* edgex, 
    const double* edgey, const double* edgedx, const double* edgedy, int* facets, 
    int* collisions, int* nparticles_sent, const int ntotal_particles, 
    const int nparticles_to_process, int* nparticles, Particle* particles_start, 
    Particle* particles_out, CrossSection* cs_scatter_table, 
    CrossSection* cs_absorb_table, double* scalar_flux_tally, 
    double* energy_deposition_tally, RNPool* rn_pool)
{
  int nparticles_out = 0;
  int nparticles_deleted = 0;

  START_PROFILING(&compute_profile);

  // Start by handling all of the initial local particles
  //#pragma omp parallel for reduction(+: facets, collisions)
  for(int pp = 0; pp < nparticles_to_process; ++pp) {
    // Current particle
    Particle* particle = 
      &particles_start[pp-nparticles_deleted];
    Particle* particle_end = 
      &particles_start[(nparticles_to_process-1)-nparticles_deleted];
    Particle* particle_out = 
      &particles_out[nparticles_out];

    const int result = handle_particle(
        global_nx, global_ny, nx, ny, x_off, y_off, neighbours, dt, initial,
        ntotal_particles, density, edgex, edgey, edgedx, edgedy, cs_scatter_table, 
        cs_absorb_table, particle_end, nparticles_sent, facets, collisions, 
        particle, particle_out, scalar_flux_tally, energy_deposition_tally, 
        rn_pool);

    nparticles_out += (result == PARTICLE_SENT);
    nparticles_deleted += (result == PARTICLE_DEAD || result == PARTICLE_SENT);
  }

  STOP_PROFILING(&compute_profile, "handling particles");

  // Correct the new total number of particles
  *nparticles -= nparticles_deleted;

  printf("handled %d particles, with %d particles deleted\n", 
      nparticles_to_process, nparticles_deleted);
}

// Handles an individual particle.
int handle_particle(
    const int global_nx, const int global_ny, const int nx, const int ny, 
    const int x_off, const int y_off, const int* neighbours, const double dt,
    const int initial, const int ntotal_particles, const double* density, 
    const double* edgex, const double* edgey, const double* edgedx, 
    const double* edgedy, const CrossSection* cs_scatter_table, 
    const CrossSection* cs_absorb_table, Particle* particle_end, 
    int* nparticles_sent, int* facets, int* collisions, Particle* particle, 
    Particle* particle_out, double* scalar_flux_tally, 
    double* energy_deposition_tally, RNPool* rn_pool)
{
  // (1) particle can stream and reach census
  // (2) particle can collide and either
  //      - the particle will be absorbed
  //      - the particle will scatter (this presumably means the energy changes)
  // (3) particle hits a boundary region and needs transferring to another process

  int x_facet = 0;
  int cs_index = -1;
  double cell_mfp = 0.0;

  // Update the cross sections, referencing into the padded mesh
  int cellx = (particle->cell%global_nx)-x_off+PAD;
  int celly = (particle->cell/global_nx)-y_off+PAD;
  double local_density = density[celly*(nx+2*PAD)+cellx];

  // This makes some assumption about the units of the data stored globally.
  // Might be worth making this more explicit somewhere.
  double microscopic_cs_scatter = 
    microscopic_cs_for_energy(cs_scatter_table, particle->e, &cs_index);
  double microscopic_cs_absorb = 
    microscopic_cs_for_energy(cs_absorb_table, particle->e, &cs_index);
  double macroscopic_cs_scatter = 
    (local_density*AVOGADROS/MOLAR_MASS)*(microscopic_cs_scatter*BARNS);
  double macroscopic_cs_absorb = 
    (local_density*AVOGADROS/MOLAR_MASS)*(microscopic_cs_absorb*BARNS);

  double particle_velocity = sqrt((2.0*particle->e*eV_TO_J)/PARTICLE_MASS);

  // Set time to census and mfps until collision, unless travelled particle
  if(initial) {
    particle->dt_to_census = dt;
    particle->mfp_to_collision = -log(genrand(rn_pool))/macroscopic_cs_scatter;
  }

  // Loop until we have reached census
  while(particle->dt_to_census > 0.0) {
    const double macroscopic_cs_total = 
      macroscopic_cs_scatter+macroscopic_cs_absorb;
    cell_mfp = 1.0/macroscopic_cs_total;

    // Work out the distance until the particle hits a facet
    double distance_to_facet = 0.0;
    calc_distance_to_facet(
        global_nx, particle->x, particle->y, x_off, y_off, particle->omega_x,
        particle->omega_y, particle_velocity, particle->cell,
        &distance_to_facet, &x_facet, edgex, edgey);

    const double distance_to_collision = particle->mfp_to_collision*cell_mfp;
    const double distance_to_census = particle_velocity*particle->dt_to_census;

    // Check if our next event is a collision
    if(distance_to_collision < distance_to_facet &&
        distance_to_collision < distance_to_census) {
      (*collisions)++;

      // Update the tallies before the energy is updated
      const double cell_volume = edgedx[cellx]*edgedy[celly];
      update_tallies(
          global_nx, nx, x_off, y_off, particle, ntotal_particles, 
          distance_to_collision, cell_volume, dt, macroscopic_cs_absorb, 
          macroscopic_cs_total, scalar_flux_tally, energy_deposition_tally);

      // The cross sections for scattering and absorbtion were calculated on 
      // a previous iteration for our given energy
      if(handle_collision(
            particle, particle_end, macroscopic_cs_absorb, macroscopic_cs_total, 
            distance_to_collision, rn_pool)) {
        return PARTICLE_DEAD;
      }

      // Energy has changed so update the cross-sections
      microscopic_cs_scatter = 
        microscopic_cs_for_energy(cs_scatter_table, particle->e, &cs_index);
      microscopic_cs_absorb = 
        microscopic_cs_for_energy(cs_absorb_table, particle->e, &cs_index);
      macroscopic_cs_scatter = 
        (local_density*AVOGADROS/MOLAR_MASS)*(microscopic_cs_scatter*BARNS);
      macroscopic_cs_absorb = 
        (local_density*AVOGADROS/MOLAR_MASS)*(microscopic_cs_absorb*BARNS);

      // Resample number of mean free paths to collision
      particle->mfp_to_collision = -log(genrand(rn_pool))/macroscopic_cs_scatter;
      particle->dt_to_census -= distance_to_collision/particle_velocity;
      particle_velocity = sqrt((2.0*particle->e*eV_TO_J)/PARTICLE_MASS);
    }
    // Check if we have reached facet
    else if(distance_to_facet < distance_to_census) {
      (*facets)++;

      // Update the mean free paths until collision
      particle->mfp_to_collision -= (distance_to_facet/cell_mfp);
      particle->dt_to_census -= (distance_to_facet/particle_velocity);

      // Update the tallies in this zone
      const double cell_volume = edgedx[cellx]*edgedy[celly];
      update_tallies(
          global_nx, nx, x_off, y_off, particle, ntotal_particles, 
          distance_to_facet, cell_volume, dt, macroscopic_cs_absorb, 
          macroscopic_cs_total, scalar_flux_tally, energy_deposition_tally);

      // Encounter facet, and jump out if particle left this rank's domain
      if(handle_facet_encounter(
            global_nx, global_ny, nx, ny, x_off, y_off, neighbours, 
            distance_to_facet, x_facet, nparticles_sent, particle, 
            particle_end, particle_out)) {
        return PARTICLE_SENT;
      }

      // Update the local density and cross-sections
      cellx = (particle->cell%global_nx)-x_off+PAD;
      celly = (particle->cell/global_nx)-y_off+PAD;

      /* We don't actually need to perform a cross sectional update here in the
       * traditional sense as we do not have to lookup the energy profile
       * in the data table, merely update it with the adjusted density. */
      local_density = 
        density[celly*(nx+2*PAD)+cellx];
      macroscopic_cs_scatter = 
        (local_density*AVOGADROS/MOLAR_MASS)*(microscopic_cs_scatter*BARNS);
      macroscopic_cs_absorb = 
        (local_density*AVOGADROS/MOLAR_MASS)*(microscopic_cs_absorb*BARNS);
    }
    // Check if we have reached census
    else {
      // We have not changed cell or energy level at this stage
      particle->x += distance_to_census*particle->omega_x;
      particle->y += distance_to_census*particle->omega_y;
      particle->mfp_to_collision -= (distance_to_census/cell_mfp);

      // Update the tallies in this zone
      const double cell_volume = edgedx[cellx]*edgedy[celly];
      update_tallies(
          global_nx, nx, x_off, y_off, particle, ntotal_particles, 
          distance_to_census, cell_volume, dt, macroscopic_cs_absorb, 
          macroscopic_cs_total, scalar_flux_tally, energy_deposition_tally);

      particle->dt_to_census = 0.0;
      break;
    }
  }

  return PARTICLE_CENSUS;
}

// Handle the collision event, including absorption and scattering
int handle_collision(
    Particle* particle, Particle* particle_end, 
    const double macroscopic_cs_absorb, const double macroscopic_cs_total, 
    const double distance_to_collision, RNPool* rn_pool)
{
  // Moves the particle to the collision site
  particle->x += distance_to_collision*particle->omega_x;
  particle->y += distance_to_collision*particle->omega_y;

  const double p_absorb = macroscopic_cs_absorb/macroscopic_cs_total;
  int is_particle_dead = 0;

  if(genrand(rn_pool) < p_absorb) {
    /* Model particle absorption */

    // Find the new particle weight after absorption, saving the energy change
    const double new_weight = particle->weight*(1.0 - p_absorb);
    particle->weight = new_weight;

    // If the particle falls below the energy of interest then we will consider
    // it dead and it will be garbage collected at some point
    if(particle->e < MIN_ENERGY_OF_INTEREST) {
      // Overwrite the particle
      *particle = *particle_end;
      is_particle_dead = 1;
    }
  }
  else {

    /* Model elastic particle scattering */

    // Choose a random scattering angle between -1 and 1
    const double mu_cm = 1.0 - 2.0*genrand(rn_pool);

    // Calculate the new energy based on the relation to angle of incidence
    // TODO: Could check here for the particular nuclide that was collided with
    const double e_new = particle->e*
      (MASS_NO*MASS_NO + 2.0*MASS_NO*mu_cm + 1.0)/
      ((MASS_NO + 1.0)*(MASS_NO + 1.0));

    // Convert the angle into the laboratory frame of reference
    double cos_theta =
      0.5*((MASS_NO+1.0)*sqrt(e_new/particle->e) - 
          (MASS_NO-1.0)*sqrt(particle->e/e_new));

    // Alter the direction of the velocities
    const double sin_theta = sin(acos(cos_theta));
    const double omega_x_new =
      (particle->omega_x*cos_theta - particle->omega_y*sin_theta);
    const double omega_y_new =
      (particle->omega_x*sin_theta + particle->omega_y*cos_theta);
    particle->omega_x = omega_x_new;
    particle->omega_y = omega_y_new;
    particle->e = e_new;
  }

  return is_particle_dead;
}

// Makes the necessary updates to the particle given that
// the facet was encountered
int handle_facet_encounter(
    const int global_nx, const int global_ny, const int nx, const int ny, 
    const int x_off, const int y_off, const int* neighbours, 
    const double distance_to_facet, int x_facet, int* nparticles_sent, 
    Particle* particle, Particle* particle_end, Particle* particle_out)
{
  // We don't need to consider the halo regions in this package
  int cellx = particle->cell%global_nx;
  int celly = particle->cell/global_nx;

  // TODO: Make sure that the roundoff is handled here, perhaps actually set it
  // fully to one of the edges here
  particle->x += distance_to_facet*particle->omega_x;
  particle->y += distance_to_facet*particle->omega_y;

  // This use of x_facet is a slight misnoma, as it is really a facet
  // along the y dimensions
  if(x_facet) {
    if(particle->omega_x > 0.0) {
      // Reflect at the boundary
      if(cellx >= (global_nx-1)) {
        particle->omega_x = -(particle->omega_x);
      }
      else {
        // Definitely moving to right cell
        cellx += 1;
        particle->cell = celly*global_nx+cellx;

        // Check if we need to pass to another process
        if(cellx >= nx+x_off) {
          send_and_replace_particle(
              neighbours[EAST], particle_end, particle, particle_out);
          nparticles_sent[EAST]++;
          return 1;
        }
      }
    }
    else if(particle->omega_x < 0.0) {
      if(cellx <= 0) {
        // Reflect at the boundary
        particle->omega_x = -(particle->omega_x);
      }
      else {
        // Definitely moving to left cell
        cellx -= 1;
        particle->cell = celly*global_nx+cellx;

        // Check if we need to pass to another process
        if(cellx < x_off) {
          send_and_replace_particle(
              neighbours[WEST], particle_end, particle, particle_out);
          nparticles_sent[WEST]++;
          return 1;
        }
      }
    }
  }
  else {
    if(particle->omega_y > 0.0) {
      // Reflect at the boundary
      if(celly >= (global_ny-1)) {
        particle->omega_y = -(particle->omega_y);
      }
      else {
        // Definitely moving to north cell
        celly += 1;
        particle->cell = celly*global_nx+cellx;

        // Check if we need to pass to another process
        if(celly >= ny+y_off) {
          send_and_replace_particle(
              neighbours[NORTH], particle_end, particle, particle_out);
          nparticles_sent[NORTH]++;
          return 1;
        }
      }
    }
    else if(particle->omega_y < 0.0) {
      // Reflect at the boundary
      if(celly <= 0) {
        particle->omega_y = -(particle->omega_y);
      }
      else {
        // Definitely moving to south cell
        celly -= 1;
        particle->cell = celly*global_nx+cellx;

        // Check if we need to pass to another process
        if(celly < y_off) {
          send_and_replace_particle(
              neighbours[SOUTH], particle_end, particle, particle_out);
          nparticles_sent[SOUTH]++;
          return 1;
        }
      }
    }
  }

  return 0;
}

// Sends a particle to a neighbour and replaces in the particle list
void send_and_replace_particle(
    const int destination, Particle* particle_end, Particle* particle, 
    Particle* particle_out)
{
#ifdef MPI
  if(destination == EDGE)
    return;

  // Place the particle in the out buffer and replace
  *particle_out = *particle;
  *particle = *particle_end;

  // Send the particle
  MPI_Send(
      particle_out, 1, particle_type, destination, TAG_PARTICLE, MPI_COMM_WORLD);
#else
  TERMINATE("Unreachable - shouldn't send particles unless MPI enabled.\n");
#endif
}

// Calculate the distance to the next facet
void calc_distance_to_facet(
    const int global_nx, const double x, const double y, const int x_off,
    const int y_off, const double omega_x, const double omega_y,
    const double particle_velocity, const int cell, double* distance_to_facet,
    int* x_facet, const double* edgex, const double* edgey)
{
  // Check the timestep required to move the particle along a single axis
  // If the velocity is positive then the top or right boundary will be hit
  const int cellx = (cell%global_nx)-x_off+PAD;
  const int celly = (cell/global_nx)-y_off+PAD;
  double u_x_inv = 1.0/(omega_x*particle_velocity);
  double u_y_inv = 1.0/(omega_y*particle_velocity);

  // The bound is open on the left and bottom so we have to correct for this and
  // required the movement to the facet to go slightly further than the edge
  // in the calculated values, using OPEN_BOUND_CORRECTION, which is the smallest
  // possible distance we can be from the closed bound e.g. 1.0e-14.
  double dt_x = (omega_x >= 0.0)
    ? ((edgex[cellx+1])-x)*u_x_inv
    : ((edgex[cellx]-OPEN_BOUND_CORRECTION)-x)*u_x_inv;
  double dt_y = (omega_y >= 0.0)
    ? ((edgey[celly+1])-y)*u_y_inv
    : ((edgey[celly]-OPEN_BOUND_CORRECTION)-y)*u_y_inv;
  *x_facet = (dt_x < dt_y) ? 1 : 0;

  // Calculated the projection to be
  // a = vector on first edge to be hit
  // u = velocity vector

  double mag_u0 = particle_velocity;

  if(*x_facet) {
    // cos(theta) = ||(x, 0)||/||(u_x', u_y')|| - u' is u at boundary
    // cos(theta) = (x.u)/(||x||.||u||)
    // x_x/||u'|| = (x_x, 0)*(u_x, u_y) / (x_x.||u||)
    // x_x/||u'|| = (x_x.u_x / x_x.||u||)
    // x_x/||u'|| = u_x/||u||
    // ||u'|| = (x_x.||u||)/u_x
    // We are centered on the origin, so the y component is 0 after travelling
    // aint the x axis to the edge (ax, 0).(x, y)
    *distance_to_facet = (omega_x >= 0.0)
      ? ((edgex[cellx+1])-x)*mag_u0*u_x_inv
      : ((edgex[cellx]-OPEN_BOUND_CORRECTION)-x)*mag_u0*u_x_inv;
  }
  else {
    // We are centered on the origin, so the x component is 0 after travelling
    // aint the y axis to the edge (0, ay).(x, y)
    *distance_to_facet = (omega_y >= 0.0)
      ? ((edgey[celly+1])-y)*mag_u0*u_y_inv
      : ((edgey[celly]-OPEN_BOUND_CORRECTION)-y)*mag_u0*u_y_inv;
  }
}

// Tallies both the scalar flux and energy deposition in the cell
void update_tallies(
    const int global_nx, const int nx, const int x_off, const int y_off, 
    Particle* particle, const int ntotal_particles, const double path_length, 
    const double cell_volume, const double dt, 
    const double macroscopic_cs_absorb, const double macroscopic_cs_total, 
    double* scalar_flux_tally, double* energy_deposition_tally)
{
  // Store the scalar flux
  const int cellx = (particle->cell%global_nx)-x_off;
  const int celly = (particle->cell/global_nx)-y_off;
  const double scalar_flux = 
    (particle->weight*path_length)/(ntotal_particles*cell_volume*dt);
  scalar_flux_tally[celly*nx+cellx] += scalar_flux; 

  // The leaving energy of a capture event is 0
  const double absorption_heating = 
    (macroscopic_cs_absorb/macroscopic_cs_total)*0.0;
  const double scattering_heating = 
    (1.0-(macroscopic_cs_absorb/macroscopic_cs_total))*particle->e*
    (MASS_NO*MASS_NO+2)/((MASS_NO+1)*(MASS_NO+1));
  energy_deposition_tally[celly*nx+cellx] += 
    eV_TO_J*scalar_flux*(particle->e-scattering_heating-absorption_heating);
}

// Fetch the cross section for a particular energy value
double microscopic_cs_for_energy(
    const CrossSection* cs, const double energy, int* cs_index)
{
  /* Attempt an optimisation of the search by performing a linear operation
   * if theere is an existing location. We assume that the energy has
   * reduced rather than increased, which seems to be a legitimate 
   * approximation in this particular case */

  int ind = 0; 
  double* key = cs->key;
  double* value = cs->value;

  // Determine the correct search direction required to move towards the
  // new energy
  const int direction = (energy-cs->value[*cs_index] > 0.0) ? 1 : -1; 

  // TODO: Determine whether this is the best approach for all cases,
  // are there situations where the linear search is not applicable, for instance
  // are thre material properties that change the energy deltas enough 
  // that the lookup jumps around? It might be worth organising the search under
  // a cost model.
  if(*cs_index > -1) {
    // This search will move in the correct direction towards the new energy group
    for(int ind = *cs_index; ind >= 0 && ind < cs->nentries; ind += direction) {
      // Check if we have found the new energy group index
      if(key[ind-1] > energy || key[ind] < energy) {
        break;
      }
    }
  }
  else {
    // Use a simple binary search to find the energy group
    ind = cs->nentries/2;
    int width = ind/2;
    while(key[ind-1] > energy || key[ind] < energy) {
      ind += (key[ind] > energy) ? -width : width;
      width = max(1, width/2); // To handle odd cases, allows one extra walk
    }
  }

  *cs_index = ind;

  // TODO: perform some interesting interpolation here
  // Center weighted is poor accuracy but might even out over enough particles
  return (value[ind-1] + value[ind])/2.0;
}

// Validates the results of the simulation
void validate(
    const int nx, const int ny, const char* params_filename, 
    const int rank, double* energy_deposition_tally)
{
  double local_energy_tally = 0.0;
  for(int ii = 0; ii < nx*ny; ++ii) {
    local_energy_tally += energy_deposition_tally[ii];
  }

  double global_energy_tally = reduce_all_sum(local_energy_tally);

  if(rank != MASTER) {
    return;
  }

  printf("\nFinal global_energy_tally %.15e\n", global_energy_tally);

  int nresults = 0;
  char* keys = (char*)malloc(sizeof(char)*MAX_KEYS*(MAX_STR_LEN+1));
  double* values = (double*)malloc(sizeof(double)*MAX_KEYS);
  if(!get_key_value_parameter(
        params_filename, NEUTRAL_TESTS, keys, values, &nresults)) {
    printf("Warning. Test entry was not found, could NOT validate.\n");
    return;
  }

  printf("Expected %.12e, result was %.12e.\n", values[0], global_energy_tally);
  if(within_tolerance(values[0], global_energy_tally, VALIDATE_TOLERANCE)) {
    printf("PASSED validation.\n");
  }
  else {
    printf("FAILED validation.\n");
  }

  free(keys);
  free(values);
}

