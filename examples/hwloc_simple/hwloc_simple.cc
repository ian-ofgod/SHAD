#include <iostream>
#include <cstdio>
#include <memory>
#include <stdexcept>
#include <string>
#include <array>
#include <sys/syscall.h>
#include "hwloc_utils.h"
#include "shad/runtime/runtime.h"


namespace shad {

int main(int argc, char** argv) {
  auto thisLoc = rt::thisLocality();

  // initialize on each locality the local_topology
  rt::executeOnAll(init_local_topo, static_cast<uint32_t>(thisLoc));

  // broadcast the localities to all the nodes
  // at the end of this call, each locality will have the view on the topology
  // of the complete system
  rt::executeOnAll(gatherRemoteTopo, -1);

  // at this point, each locality should have all the topologies in the system
  // save all of them (the view of all localities from each locality)
  rt::executeOnAll(
      [](const int& UNUSED) {
        // saving all the topologies received from the different localities
        for (auto i : topology_map) {
          std::string filename =
              "./rank-" +
              std::to_string(static_cast<uint32_t>(rt::thisLocality())) +
              "-view_of-rank-" + std::to_string(i.first) + ".xml";
          hwloc_topology_export_xml(i.second, filename.data(), 0);
        }

        std::cout << rt::thisLocality() << " done" << std::endl;
      },
      -1);

  return 0;
}

}  // namespace shad