constexpr operator_for operator|(operator_for a, operator_for b){
  return static_cast<operator_for>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
};

constexpr operator_for& operator|=(operator_for &lhs, operator_for rhs){
    lhs = lhs | rhs;
    return lhs;
};

constexpr operator_for operator&(operator_for a, operator_for b)
{
    return static_cast<operator_for>(
        static_cast<uint32_t>(a) & static_cast<uint32_t>(b)
    );
};

constexpr operator_for& operator&=(operator_for &lhs, operator_for rhs)
{
    lhs = lhs & rhs;
    return lhs;
};

constexpr operator_for operator~(operator_for a){
  return static_cast<operator_for>(~(static_cast<uint32_t>(a)));
};


constexpr static inline bool ep_has_flag(operator_for val,operator_for flag){
  return ((static_cast<uint32_t>(val) & static_cast<uint32_t>(flag)) != 0);
};

