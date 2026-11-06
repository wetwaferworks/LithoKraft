#include "torch.h"
#include "components.h"

namespace lithokraft::components {

void registerTorch(const Registrar& add) {
  add("torch", std::make_unique<cTorch>());
}

}  // namespace lithokraft::components
