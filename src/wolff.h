// wolff.h

#pragma once

#include <vector>

class QfeLattice;

class QfeWolff {

 public:
  void Init(QfeLattice* lattice);
  int Update();
  inline int ClusterSize() { return cluster.size(); }

 protected:
  bool TestSite(int s, double test_value);
  void AddToCluster(int s);
  void FlipCluster();

 private:
  QfeLattice* lattice;
  std::vector<bool> is_clustered;  // keeps track of which sites are clustered
  std::vector<int> cluster;  // array of clustered sites
};
