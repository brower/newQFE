// ads3_crit.cc

#include <getopt.h>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>
#include "ads3.h"
#include "phi4.h"
#include "statistics.h"

int main(int argc, char* argv[]) {

  // default parameters
  int n_layers = 3;
  int q = 7;
  int Nt = 0;  // default to number of boundary sites
  double msq = -1.0;
  double lambda = 1.0;
  int n_therm = 1000;
  int n_traj = 20000;
  int n_skip = 20;
  int n_wolff = 4;
  int n_metropolis = 1;
  double metropolis_z = 0.1;

  const struct option long_options[] = {
    { "n_layers", required_argument, 0, 'N' },
    { "q", required_argument, 0, 'q' },
    { "n_t", required_argument, 0, 'T' },
    { "msq", required_argument, 0, 'm' },
    { "lambda", required_argument, 0, 'l' },
    { "n_therm", required_argument, 0, 'h' },
    { "n_traj", required_argument, 0, 't' },
    { "n_skip", required_argument, 0, 's' },
    { "n_wolff", required_argument, 0, 'w' },
    { "n_metropolis", required_argument, 0, 'e' },
    { "metropolis_z", required_argument, 0, 'z' },
    { 0, 0, 0, 0 }
  };

  const char* short_options = "NqTmlhtswez";

  while (true) {

    int o = 0;
    int c = getopt_long(argc, argv, short_options, long_options, &o);
    if (c == -1) break;

    switch (c) {
      case 'N': n_layers = atoi(optarg); break;
      case 'q': q = atoi(optarg); break;
      case 'T': Nt = atoi(optarg); break;
      case 'm': msq = std::stod(optarg); break;
      case 'l': lambda = std::stod(optarg); break;
      case 'h': n_therm = atoi(optarg); break;
      case 't': n_traj = atoi(optarg); break;
      case 's': n_skip = atoi(optarg); break;
      case 'w': n_wolff = atoi(optarg); break;
      case 'e': n_metropolis = atoi(optarg); break;
      case 'z': metropolis_z = std::stod(optarg); break;
      default: break;
    }
  }

  printf("n_therm: %d\n", n_therm);
  printf("n_traj: %d\n", n_traj);
  printf("n_skip: %d\n", n_skip);
  printf("n_wolff: %d\n", n_wolff);
  printf("n_metropolis: %d\n", n_metropolis);

  QfeLatticeAdS3 lattice(n_layers, q, Nt);
  printf("n_layers: %d\n", lattice.n_layers);
  printf("q: %d\n", lattice.q);
  printf("Nt: %d\n", lattice.Nt);
  printf("total sites: %d\n", lattice.n_sites + lattice.n_dummy);
  printf("bulk sites: %d\n", lattice.n_bulk);
  printf("boundary sites: %d\n", lattice.n_boundary);
  printf("dummy sites: %d\n", lattice.n_dummy);
  printf("t_scale: %.12f\n", lattice.t_scale);

  printf("average rho/cosh(rho) at each layer:\n");
  for (int n = 0; n <= n_layers + 1; n++) {
    printf("%d %.12f %.12f %.12f\n", n, \
        lattice.layer_rho[n], \
        lattice.layer_cosh_rho[n], \
        lattice.total_cosh_rho[n]);
  }

  QfePhi4 field(&lattice, msq, lambda);
  field.metropolis_z = metropolis_z;
  field.HotStart();
  printf("msq: %.4f\n", field.msq);
  printf("lambda: %.4f\n", field.lambda);
  printf("metropolis_z: %.4f\n", field.metropolis_z);
  printf("initial action: %.12f\n", field.Action());

  // measurements
  std::vector<double> phi;  // average phi on the boundary (magnetization)
  std::vector<double> phi2;  // average phi^2 on the boundary
  std::vector<double> phi_abs;  // average abs(phi) on the boundary
  std::vector<double> action;
  QfeMeasReal cluster_size;
  QfeMeasReal accept_metropolis;
  QfeMeasReal accept_overrelax;
  QfeMeasReal demon;

  for (int n = 0; n < (n_traj + n_therm); n++) {

    int cluster_size_sum = 0;
    for (int j = 0; j < n_wolff; j++) {
      cluster_size_sum += field.WolffUpdate();
    }
    double metropolis_sum = 0.0;
    for (int j = 0; j < n_metropolis; j++) {
      metropolis_sum += field.Metropolis();
    }
    cluster_size.Measure(double(cluster_size_sum) / double(lattice.n_sites));
    accept_metropolis.Measure(metropolis_sum);
    accept_overrelax.Measure(field.Overrelax());

    if (n % n_skip || n < n_therm) continue;

    demon.Measure(field.overrelax_demon);

    // measure <phi> on the boundary
    double phi_sum = 0.0;
    double phi2_sum = 0.0;
    double phi_abs_sum = 0.0;
    for (int i = 0; i < lattice.n_boundary; i++) {
      int s = lattice.boundary_sites[i];
      phi_sum += field.phi[s] * lattice.sites[s].wt;
      phi2_sum += field.phi[s] * field.phi[s] * lattice.sites[s].wt;
      phi_abs_sum += fabs(field.phi[s]) * lattice.sites[s].wt;
    }
    phi.push_back(phi_sum / double(lattice.n_boundary));
    phi2.push_back(phi2_sum / double(lattice.n_boundary));
    phi_abs.push_back(phi_abs_sum / double(lattice.n_boundary));

    action.push_back(field.Action());
    printf("%06d %.12f %.4f %.4f %.12f %.4f\n", \
        n, action.back(), \
        accept_metropolis.last, \
        accept_overrelax.last, demon.last, \
        cluster_size.last);
  }

  printf("cluster_size/V: %.4f\n", cluster_size.Mean());
  printf("accept_metropolis: %.4f\n", accept_metropolis.Mean());
  printf("accept_overrelax: %.4f\n", accept_overrelax.Mean());
  printf("demon: %.12f (%.12f)\n", demon.Mean(), demon.Error());

  std::vector<double> mag_abs(phi.size());
  std::vector<double> mag2(phi.size());
  std::vector<double> mag4(phi.size());
  std::vector<double> mag_action(phi.size());
  std::vector<double> mag2_action(phi.size());
  std::vector<double> mag3_action(phi.size());
  std::vector<double> mag4_action(phi.size());
  for (int i = 0; i < phi.size(); i++) {
    double m = phi[i];
    double m2 = m * m;
    mag_abs[i] = fabs(m);
    mag2[i] = m2;
    mag4[i] = m2 * m2;
    mag_action[i] = m * action[i];
    mag2_action[i] = m2 * action[i];
    mag3_action[i] = m * m2 * action[i];
    mag4_action[i] = m2 * m2 * action[i];
  }

  printf("phi: %+.12e (%.12e), %.4f\n", \
      Mean(phi), JackknifeMean(phi), \
      AutocorrTime(phi));
  printf("phi^2: %.12e (%.12e), %.4f\n", \
      Mean(phi2), JackknifeMean(phi2), \
      AutocorrTime(phi2));
  printf("phi_abs: %.12e (%.12e), %.4f\n", \
      Mean(phi_abs), JackknifeMean(phi_abs), \
      AutocorrTime(phi_abs));
  printf("phi_susc: %.12e (%.12e)\n", \
      Susceptibility(phi2, phi_abs), \
      JackknifeSusceptibility(phi2, phi_abs));

  printf("m: %+.12e (%.12e), %.4f\n", \
      Mean(phi), JackknifeMean(phi), \
      AutocorrTime(phi));
  printf("m^2: %.12e (%.12e), %.4f\n", \
      Mean(mag2), JackknifeMean(mag2), \
      AutocorrTime(mag2));
  printf("m^4: %.12e (%.12e), %.4f\n", \
      Mean(mag4), JackknifeMean(mag4), \
      AutocorrTime(mag4));
  printf("U4: %.12e (%.12e)\n", \
      U4(mag2, mag4), \
      JackknifeU4(mag2, mag4));
  printf("m_susc: %.12e (%.12e)\n", \
      Susceptibility(mag2, mag_abs), \
      JackknifeSusceptibility(mag2, mag_abs));

  printf("S: %+.12e (%.12e), %.4f\n", \
      Mean(action), JackknifeMean(action), \
      AutocorrTime(action));
  printf("m_S: %+.12e (%.12e), %.4f\n", \
      Mean(mag_action), JackknifeMean(mag_action), \
      AutocorrTime(mag_action));
  printf("m^2_S: %.12e (%.12e), %.4f\n", \
      Mean(mag2_action), JackknifeMean(mag2_action), \
      AutocorrTime(mag2_action));
  printf("m^3_S: %.12e (%.12e), %.4f\n", \
      Mean(mag3_action), JackknifeMean(mag3_action), \
      AutocorrTime(mag3_action));
  printf("m^4_S: %.12e (%.12e), %.4f\n", \
      Mean(mag4_action), JackknifeMean(mag4_action), \
      AutocorrTime(mag4_action));

  FILE* file;

  file = fopen("ads3_crit_boundary.dat", "a");
  fprintf(file, "%d", lattice.n_layers);
  fprintf(file, " %d", lattice.Nt);
  fprintf(file, " %.12f", field.msq);
  fprintf(file, " %.4f", field.lambda);
  fprintf(file, " %+.12e %.12e", Mean(phi), JackknifeMean(phi));
  fprintf(file, " %.12e %.12e", Mean(phi2), JackknifeMean(phi2));
  fprintf(file, " %.12e %.12e", Mean(phi_abs), JackknifeMean(phi_abs));
  fprintf(file, " %.12e %.12e", \
      Susceptibility(phi2, phi_abs), JackknifeSusceptibility(phi2, phi_abs));
  fprintf(file, " %.12e %.12e", Mean(mag2), JackknifeMean(mag2));
  fprintf(file, " %.12e %.12e", Mean(mag_abs), JackknifeMean(mag_abs));
  fprintf(file, " %.12e %.12e", U4(mag2, mag4), JackknifeU4(mag2, mag4));
  fprintf(file, " %.12e %.12e", \
      Susceptibility(mag2, mag_abs), JackknifeSusceptibility(mag2, mag_abs));
  fprintf(file, " %.12e %.12e", Mean(action), JackknifeMean(action));
  fprintf(file, " %.12e %.12e", Mean(mag_action), JackknifeMean(mag_action));
  fprintf(file, " %.12e %.12e", Mean(mag2_action), JackknifeMean(mag2_action));
  fprintf(file, " %.12e %.12e", Mean(mag3_action), JackknifeMean(mag4_action));
  fprintf(file, " %.12e %.12e", Mean(mag4_action), JackknifeMean(mag4_action));
  fprintf(file, "\n");
  fclose(file);

  return 0;
}
