#include "mim/def.h"

#include "mim/plug/spirv/be/emit.h"
namespace mim::plug::spirv {

class TypeConverter {
    TypeConverter(World&);

    Word convert(const Def* type);

private:
    DefMap<Word> ids_;
};

} // namespace mim::plug::spirv
