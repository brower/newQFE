// ads3.h

#pragma once

#include <vector>
#include "ads2.h"

class QfeLatticeAdS3 : public QfeLatticeAdS2 {

public:
  QfeLatticeAdS3(int n_layers, int q, int Nt);
  double Sigma(int s1, int s2);
  double DeltaT(int s1, int s2);

  int Nt;
  double t_scale;  // ratio of temporal to spatial lattice spacing
  std::vector<int> t;  // time coordinate of sites
};

/**
 * @brief Initialize a triangulated lattice on AdS3.
 *
 * A Poincaré disk is used to generate an AdS2 slice of the lattice. Then
 * additional slices are created and spaced appropriately in the time
 * direction.
 *
 * [1] R. Brower et al., Phys. Rev. D, 103, 094507 (2021).
 * @see https://arxiv.org/abs/1912.07606
 *
 * @param n_layers Number of layers to create
 * @param q Number of triangles meeting at each site (should be greater than 6)
 * @param Nt Number of time slices
 */

QfeLatticeAdS3::QfeLatticeAdS3(int n_layers, int q, int Nt) :
  QfeLatticeAdS2(n_layers, q) {

  this->Nt = Nt;

  // we start with a single AdS2 slice, and we need to make Nt copies
  int n_sites_slice = layer_offset[n_layers + 1];  // dynamic sites per slice
  n_sites = n_sites_slice * Nt;  // number of dynamic sites
  sites.resize(n_sites + n_dummy);
  site_layers.resize(sites.size());

  // resize coordinate arrays
  z.resize(sites.size());
  r.resize(sites.size());
  theta.resize(sites.size());
  rho.resize(sites.size());
  u.resize(sites.size());
  t.resize(sites.size(), 0);

  // set temporal-spatial lattice spacing ratio
  t_scale = 1.0;
  // t_scale = 1.0 / layer_rho[1];
  // t_scale = layer_cosh_rho[1];
  // t_scale = double(n_layers) / (layer_cosh_rho[n_layers] * M_PI);
  // t_scale = layer_cosh_rho[n_layers];
  // t_scale = total_cosh_rho[n_layers];
  // t_scale = double(2 * n_layers) / (M_PI * layer_rho[n_layers]);

  // move the dummy sites to the end of the array
  layer_sites[n_layers + 1].clear();
  for (int i = 0; i < n_dummy; i++) {
    int s_old = n_sites_slice + i;
    int s_new = n_sites + i;
    sites[s_new].nn = 0;
    sites[s_new].wt = cosh(rho[s_old]);
    layer_sites[n_layers + 1].push_back(s_new);

    // copy coordinates
    z[s_new] = z[s_old];
    r[s_new] = r[s_old];
    theta[s_new] = theta[s_old];
    rho[s_new] = rho[s_old];
    u[s_new] = u[s_old];
    t[s_new] = 0;  // all dummy sites are at t = 0
  }

  // duplicate sites on the other slices
  for (int s0 = 0; s0 < n_sites_slice; s0++) {
    QfeSite* site0 = &sites[s0];

    // adjust the site weight
    site0->wt = cosh(rho[s0]) / t_scale;
    int layer = site_layers[s0];

    // adjust neighbor table for dummy neighbors
    for (int n = 0; n < site0->nn; n++) {
      int s = site0->neighbors[n];
      if (s >= n_sites_slice) {
        site0->neighbors[n] = n_sites + (s - n_sites_slice);
      }
    }

    // copy site coordinates and weights
    for (int tt = 1; tt < Nt; tt++) {
      int s = n_sites_slice * tt + s0;
      sites[s].nn = 0;  // add links later
      sites[s].wt = site0->wt;

      site_layers[s] = layer;
      layer_sites[layer].push_back(s);
      if (layer < n_layers) {
        bulk_sites.push_back(s);
      } else if (layer == n_layers) {
        boundary_sites.push_back(s);
      }

      z[s] = z[s0];
      r[s] = r[s0];
      theta[s] = theta[s0];
      rho[s] = rho[s0];
      u[s] = u[s0];
      t[s] = tt;
    }
  }
  n_bulk = bulk_sites.size();
  n_boundary = boundary_sites.size();

  // duplicate links on the other slices
  int n_links_slice = links.size();
  for (int l = 0; l < n_links_slice; l++) {

    int s_a = links[l].sites[0];
    int s_b = links[l].sites[1];

    // adjust neighbor table for dummy neighbors
    if (s_a >= n_sites_slice) {
      s_a = n_sites + (s_a - n_sites_slice);
      links[l].sites[0] = s_a;
    }
    if (s_b >= n_sites_slice) {
      s_b = n_sites + (s_b - n_sites_slice);
      links[l].sites[1] = s_b;
    }

    // adjust the link weight
    double link_wt = 0.5 * (cosh(rho[s_a]) + cosh(rho[s_b])) / t_scale;
    links[l].wt = link_wt;

    // add links in the other slices
    for (int tt = 1; tt < Nt; tt++) {
      if (s_a < n_sites) {
        s_a = (s_a + n_sites_slice) % n_sites;
      }
      if (s_b < n_sites) {
        s_b = (s_b + n_sites_slice) % n_sites;
      }
      AddLink(s_a, s_b, link_wt);
    }
  }

  // add links to connect the time slices with periodic boundary conditions
  for (int s = 0; s < n_sites; s++) {
    AddLink(s, (s + n_sites_slice) % n_sites, t_scale / cosh(rho[s]));
  }
}

double QfeLatticeAdS3::Sigma(int s1, int s2) {
  if (s1 == s2) return 0.0;
  double rho1 = rho[s1];
  double rho2 = rho[s2];
  double dt = DeltaT(s1, s2);
  double theta = Theta(s1, s2);
  double x1 = cosh(dt) * cosh(rho1) * cosh(rho2);
  double x2 = cos(theta) * sinh(rho1) * sinh(rho2);
  return acosh(x1 - x2);
}

double QfeLatticeAdS3::DeltaT(int s1, int s2) {
  int dt = (Nt / 2) - abs(abs(t[s1] - t[s2]) - (Nt / 2));
  return double(dt) / t_scale;
}