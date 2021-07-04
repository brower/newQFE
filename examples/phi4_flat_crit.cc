// phi4_flat_crit.cc

#include <getopt.h>
#include <cstdio>
#include <string>
#include <vector>
#include "phi4.h"
#include "statistics.h"

int main(int argc, char* argv[]) {

  // default parameters
  int N = 64;
  double skew = 1.0;
  double msq = -1.27;
  double lambda = 0.25;
  int n_therm = 1000;
  int n_traj = 20000;
  int n_skip = 20;
  int n_wolff = 4;
  int n_metropolis = 1;
  double metropolis_z = 0.1;

  const struct option long_options[] = {
    { "N", required_argument, 0, 'N' },
    { "skew", required_argument, 0, 'k' },
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

  const char* short_options = "Nkmlhtswez";

  while (true) {

    int o = 0;
    int c = getopt_long(argc, argv, short_options, long_options, &o);
    if (c == -1) break;

    switch (c) {
      case 'N': N = atoi(optarg); break;
      case 'k': skew = std::stod(optarg); break;
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

  QfeLattice lattice;
  lattice.InitTriangle(N, skew);
  printf("N: %d\n", N);
  printf("skew: %.2f\n", skew);

  QfePhi4 field(&lattice, msq, lambda);
  field.HotStart();
  printf("msq: %.4f\n", msq);
  printf("lambda: %.4f\n", lambda);
  printf("metropolis_z: %.4f\n", field.metropolis_z);
  printf("initial action: %.12f\n", field.Action());

  // measurements
  std::vector<double> phi;  // average phi on the boundary (magnetization)
  std::vector<double> phi2;  // average phi^2 on the boundary
  std::vector<double> phi_abs;  // average abs(phi) on the boundary
  std::vector<double> action;
  QfeMeasReal demon;
  QfeMeasReal cluster_size;
  QfeMeasReal accept_metropolis;
  QfeMeasReal accept_overrelax;

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

    // measure <phi>
    double phi_sum = 0.0;
    double phi2_sum = 0.0;
    double phi_abs_sum = 0.0;
    for (int s = 0; s < lattice.n_sites; s++) {
      phi_sum += field.phi[s] * lattice.sites[s].wt;
      phi2_sum += field.phi[s] * field.phi[s] * lattice.sites[s].wt;
      phi_abs_sum += abs(field.phi[s]) * lattice.sites[s].wt;
    }
    phi.push_back(phi_sum / double(lattice.n_sites));
    phi2.push_back(phi2_sum / double(lattice.n_sites));
    phi_abs.push_back(phi_abs_sum / double(lattice.n_sites));

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
  std::vector<double> mag3(phi.size());
  std::vector<double> mag4(phi.size());
  for (int i = 0; i < phi.size(); i++) {
    double m = phi[i];
    double m2 = m * m;
    mag_abs[i] = abs(m);
    mag2[i] = m2;
    mag3[i] = m * m2;
    mag4[i] = m2 * m2;
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

  FILE* file;

  file = fopen("phi4_flat_crit.dat", "a");
  fprintf(file, "%d", N);
  fprintf(file, " %.12f", skew);
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
  fprintf(file, "\n");
  fclose(file);

  return 0;
}