#include <iostream>
#include <cstdio>
#include <memory>
#include <stdexcept>
#include <string>
#include <array>
#include <sys/syscall.h>
#include "hwloc_utils.h"
#include "shad/runtime/runtime.h"



std::map<int, hwloc_cpuset_t> gpu_cpuset_map() {
  /* Find CUDA devices through the corresponding OS devices */
  int n = hwloc_get_nbobjs_by_type(local_topology, HWLOC_OBJ_OS_DEVICE);
  std::map<int, hwloc_cpuset_t> cpuset_map;

  for (int i = 0; i < n; i++) {
    const char* s;
    hwloc_obj_t obj = hwloc_get_obj_by_type(local_topology, HWLOC_OBJ_OS_DEVICE, i);

    s = hwloc_obj_get_info_by_name(obj, "Backend");

    if (s && !strcmp(s, "RSMI")) {
      int devid = atoi(obj->name + 4);  // get the cudaRT deviceID for the current GPU (cudaX)
      printf("RSMI device %d\n", devid);

      hwloc_cpuset_t cpuset_gpu = hwloc_bitmap_alloc();
      int i = hwloc_rsmi_get_device_cpuset(local_topology, devid, cpuset_gpu);
      if(i <0)
        std::cout<< "STFU" << std::endl;
      displayCpuSet(cpuset_gpu, "hwloc_rsmi_get_device_cpuset", static_cast<uint32_t>(shad::rt::thisLocality()));
      cpuset_map.insert({devid, cpuset_gpu});
    }
  }
  return cpuset_map;
}

std::map<int, std::vector<hwloc_obj_t>> gpuCpuSet_to_gpuCorelist(
    std::map<int, hwloc_cpuset_t> gpu_cpuset_map) {
  std::map<int, std::vector<hwloc_obj_t>> res;
  for (auto elem : gpu_cpuset_map) {
    // get the number of physical cores inside the current cpuset
    int n_cores = hwloc_get_nbobjs_inside_cpuset_by_type(
        local_topology, elem.second, HWLOC_OBJ_CORE);

    hwloc_obj_t core = hwloc_get_next_obj_inside_cpuset_by_type(
        local_topology, elem.second, HWLOC_OBJ_CORE, NULL);
    std::vector<hwloc_obj_t> curr_gpu_core_vect;
    while (core != NULL) {
      curr_gpu_core_vect.push_back(core);  // saves a copy of core
      core = hwloc_get_next_obj_inside_cpuset_by_type(
          local_topology, elem.second, HWLOC_OBJ_CORE, core);
    }

    res.insert({elem.first, curr_gpu_core_vect});
  }

  return res;
}

std::map<int, hwloc_cpuset_t> singlified_gpu_cpuset_map() {
  /* Find CUDA devices through the corresponding OS devices */
  int n = hwloc_get_nbobjs_by_type(local_topology, HWLOC_OBJ_OS_DEVICE);
  std::map<int, hwloc_cpuset_t> cpuset_map;

  for (int i = 0; i < n; i++) {
    const char* s;
    hwloc_obj_t obj =
        hwloc_get_obj_by_type(local_topology, HWLOC_OBJ_OS_DEVICE, i);
    // printf("%s:\n", obj->name);

    s = hwloc_obj_get_info_by_name(obj, "Backend");
    /* obj->subtype also contains CUDA or OpenCL since v2.0 */

    if (s && !strcmp(s, "RSMI")) {
      int devid =
          atoi(obj->name +
               4);  // get the cudaRT deviceID for the current GPU (cudaX)
      printf("RSMI device %d\n", devid);

      hwloc_cpuset_t cpuset_gpu = hwloc_bitmap_alloc();
      hwloc_rsmi_get_device_cpuset(local_topology, devid, cpuset_gpu);
      hwloc_bitmap_singlify_per_core(local_topology, cpuset_gpu, 0);
      displayCpuSet(cpuset_gpu, "(singlified) hwloc_rsmi_get_device_cpuset",
                    static_cast<uint32_t>(shad::rt::thisLocality()));
      cpuset_map.insert({devid, cpuset_gpu});
    }
  }
  return cpuset_map;
}

std::map<int, std::vector<hwloc_obj_t>> gpu_numalist_map() {
  /* Find CUDA devices through the corresponding OS devices */
  int n = hwloc_get_nbobjs_by_type(local_topology, HWLOC_OBJ_OS_DEVICE);
  std::map<int, std::vector<hwloc_obj_t>> numalist_map;

  for (int i = 0; i < n; i++) {
    const char* s;

    /* TODO: THIS FAILS if GPUs in the current system are place at different
     * depth levels (is it possible??)*/
    hwloc_obj_t obj =
        hwloc_get_obj_by_type(local_topology, HWLOC_OBJ_OS_DEVICE, i);

    // printf("%s:\n", obj->name);

    s = hwloc_obj_get_info_by_name(obj, "Backend");
    /* obj->subtype also contains CUDA or OpenCL since v2.0 */

    if (s && (!strcmp(s, "CUDA") || !strcmp(s, "RSMI"))) {
      // get the GPU device id (rsmiX, cudaX)
      // TODO: NOTE! heterogenous GPU systems could have problems since multiple
      // devID could coincide
      int devid = atoi(obj->name + 4);

      // get the first ancestor with a nodeset attribute
      hwloc_obj_t ancestor = obj;
      while (ancestor->nodeset == NULL) {
        ancestor = ancestor->parent;
        if (ancestor == NULL)  // something bad happened, no NUMA?
          break;
      }
      if (ancestor == NULL) continue;

      hwloc_nodeset_t nodeset = ancestor->nodeset;
      std::vector<hwloc_obj_t> numalist;
      unsigned index;
      hwloc_bitmap_foreach_begin(index, nodeset) numalist.push_back(
          hwloc_get_numanode_obj_by_os_index(local_topology, index));
      hwloc_bitmap_foreach_end();

      numalist_map.insert({devid, numalist});
    }
  }

  return numalist_map;
}

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

        std::cout << "======================" << std::endl;
        std::cout << "Singlified PUs (one per core)" << std::endl;
        std::cout << "======================" << std::endl;
        auto system_cpuset = hwloc_topology_get_complete_cpuset(local_topology);
        auto modifiable_cpuset = hwloc_bitmap_dup(system_cpuset);
        hwloc_bitmap_singlify_per_core(local_topology, modifiable_cpuset, 0);  // 0 means keep the first PU for each core
        unsigned index;
        hwloc_bitmap_foreach_begin(index, modifiable_cpuset)
		  		std::cout<< "PU(one_per_core): " << index << std::endl;			
        hwloc_bitmap_foreach_end();

        std::cout << "======================" << std::endl;
        std::cout << "cpuset associated with each GPU" << std::endl;
        std::cout << "======================" << std::endl;
        auto gpu_cpusets = gpu_cpuset_map();

        std::cout << "======================" << std::endl;
        std::cout << "Physical cores (and PUs) inside each GPU_cpuset"
                  << std::endl;
        std::cout << "======================" << std::endl;
        auto gpu_cores_map = gpuCpuSet_to_gpuCorelist(gpu_cpusets);
        for (auto elem : gpu_cores_map) {
          std::cout << rt::thisLocality() << " GPU_" << elem.first << std::endl;
          for (auto core : elem.second) {
            std::string title = "core_" + std::to_string(core->os_index);
            displayCpuSet(core->cpuset, title.data(),
                          static_cast<uint32_t>(rt::thisLocality()));
          }
        }

        std::cout << "======================" << std::endl;
        std::cout << "Singlified PUs for each core of GPU_cpuset" << std::endl;
        std::cout << "======================" << std::endl;
        auto singlified = singlified_gpu_cpuset_map();

        std::cout << "======================" << std::endl;
        std::cout << "NUMA OBJs for each GPU" << std::endl;
        std::cout << "======================" << std::endl;
        auto gpus_nodes = gpu_numalist_map();
        for (auto elem : gpus_nodes) {
          std::cout << rt::thisLocality() << " GPU_" << elem.first << std::endl;
          for (auto numa : elem.second) {
            std::cout << rt::thisLocality() << "  numa: " << numa->os_index
                      << std::endl;
          }
        }
			std::cout << "======================" << std::endl;
        std::cout << "======================" << std::endl;


        std::cout << std::endl;
      },
      -1);

  return 0;
}

}  // namespace shad