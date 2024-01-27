#include <iostream>
#include <cstdio>
#include <memory>
#include <stdexcept>
#include <string>
#include <array>
#include <sys/syscall.h>
#include "shad/runtime/runtime.h"


//#include "/home/didi959/hwloc/install/include/hwloc.h" // TODO: REMOVE absolute path!! Only needed for VSCode intellisense bug
#include "hwloc.h"
//#include "/home/didi959/hwloc/install/include/hwloc/glibc-sched.h" // TODO: REMOVE absolute path!! Only needed for VSCode intellisense bug
#include "hwloc/glibc-sched.h"
//#include "/home/didi959/hwloc/install/include/hwloc/cuda.h" // TODO: REMOVE absolute path!! Only needed for VSCode intellisense bug
#include "hwloc/rsmi.h"



using out_args_t = std::tuple<const char*, uint32_t>;

// EACH LOCALITY HAS ITS OWN
hwloc_topology_t local_topology; // local hwloc topology
char* local_topo_xml; //used to set the local_topology to the other localities
int local_topo_xml_len; // len of the local topo xml
std::map<int, hwloc_topology_t> topology_map;  // LOCAL map between a rank and its topology
// ===========================

void get_remote_topo_xml(const int& unused, char* ret_buffer,
                         uint32_t* ret_buffer_size) {
  memcpy(ret_buffer, local_topo_xml, local_topo_xml_len);
  *ret_buffer_size = local_topo_xml_len;
}


void get_remote_topo_info(const int& UNUSED, out_args_t* outArgs) {
  std::get<0>(*outArgs) = local_topo_xml;
  std::get<1>(*outArgs) = local_topo_xml_len;
}

void gatherRemoteTopo(const int& UNUSED) {
  for (auto& loc : shad::rt::allLocalities()) {
    // get buff size on remote locality
    out_args_t outArgs;

    if (loc == shad::rt::thisLocality()) {
      // avoid doing remote calls if we are in the current locality
      topology_map.insert({static_cast<uint32_t>(loc), local_topology});
    } else {
      shad::rt::executeAtWithRet(loc, get_remote_topo_info, -1, &outArgs);
      int size = std::get<1>(outArgs);
      const char* remotePtr = std::get<0>(outArgs);

      // allocate buff of the correct size
      std::unique_ptr<char[]> outbuff(new char[size]);

      // issue a dma
      shad::rt::dma(outbuff.get(), loc, remotePtr, size);

      // reconstruct the topology from the XML (easy query for info)
      hwloc_topology_t cTop;
      hwloc_topology_init(&cTop);  // allocate the context
      hwloc_topology_set_io_types_filter(cTop, HWLOC_TYPE_FILTER_KEEP_ALL);

      // CHECK FLAG TO SET THE CURRENT PROCESS

      hwloc_topology_set_xmlbuffer(cTop, outbuff.get(),
                                   size);  // specify where to read the topology
      hwloc_topology_load(cTop);           // read the actual topology

      // add the topology to the map for later use
      topology_map.insert({static_cast<uint32_t>(loc), cTop});
    }
  }
}


// usefull to print the general structure
static void hwloc_print_children(hwloc_topology_t topology, hwloc_obj_t obj, int depth){
    if(depth==2){
        return; // debugging -> set the depth to display to stop at 2
    }
    char type[32], attr[1024];
    unsigned i;

    hwloc_obj_type_snprintf(type, sizeof(type), obj, 0);
    printf("%*s%s", 2*depth, "", type);
    if (obj->os_index != (unsigned) -1)
      printf("#%u", obj->os_index);
    hwloc_obj_attr_snprintf(attr, sizeof(attr), obj, " ", 0);
    if (*attr)
      printf("(%s)", attr);
    printf("\n");
    for (i = 0; i < obj->arity; i++) {
         hwloc_print_children(topology, obj->children[i], depth + 1);
    }
}

// init local_topology var
void init_local_topo(const uint32_t& ID){
    hwloc_topology_init(&local_topology);  // initialization
    hwloc_topology_set_io_types_filter (local_topology, HWLOC_TYPE_FILTER_KEEP_ALL);
    hwloc_topology_set_pid(local_topology, getpid());
    hwloc_topology_load(local_topology);   // actual detection
    hwloc_topology_export_xmlbuffer(local_topology, &local_topo_xml, &local_topo_xml_len, 0);
}

void displayCpuSet(hwloc_cpuset_t set, const char* bitmap_description, uint32_t locality){
  char type[128];
  char* bitmap;
  hwloc_bitmap_asprintf(&bitmap, set);
  std::cout << "Locality[" << locality << "]"
            << " " << bitmap_description << ": "
            << bitmap <<" size: "<<   hwloc_bitmap_weight(set) << "PUs" << std::endl;
  unsigned int id;
  hwloc_bitmap_foreach_begin(id, set) 
      hwloc_obj_t obj = hwloc_get_pu_obj_by_os_index(local_topology, id);
      hwloc_obj_type_snprintf(type, sizeof(type), obj, 1);
      std::cout   << "Locality[" << locality << "] "
                  << "      type: " << type 
                  << "  gp_id: " << obj->gp_index // general purpose id (incremental and UNIQUE)
                  << "  real: " << id // the bitmap is referred to the physical ids
                  << "  logical " << obj->logical_index << std::endl; // the hwloc only logical id (platform/bios/update independent)
  hwloc_bitmap_foreach_end();
}