{-# OPTIONS_GHC -Wno-incomplete-patterns #-}

bool2bit :: Bool -> Int
bool2bit x = if x then 1 else 0

f_1172733 :: (Int, Int -> a) -> a
f_1172733 _1172776@(_1172778, _1172801) =
  let _1172799 _1172802 = _1172801 _1172802
      pow_cont_1172794 __1172827 =
        let _1172834 = 42 * __1172827
         in _1172799 _1172834
      pow_else_1172757 _ =
        let _1172785 = -1 + _1172778
         in f_1172733 (_1172785, pow_cont_1172794)
      pow_then_1172846 _ = _1172799 1
      _1172872 = bool2bit (0 == _1172778)
   in ([pow_else_1172757, pow_then_1172846] !! _1172872) ()

mainThorin :: ((), (), (), ((), Int) -> a) -> a
mainThorin (mem_1172918, _, _, return_1172903) =
  let return_1172898 _1172904 = return_1172903 _1172904
      ret_cont_1172889 r_1172922 = return_1172898 (mem_1172918, r_1172922)
   in f_1172733 (2, ret_cont_1172889)

pow :: ([Int], Int -> a) -> a
pow (__1172984@[a_1172985, b_1172989], ret_1173011) =
  let ret_1173009 _1173012 = ret_1173011 _1173012
      pow_cont_1173002 __1173020 =
        let _1173025 = head __1172984 * __1173020
         in ret_1173009 _1173025
      pow_else_1172967 _ =
        let _1172996 = -1 + (__1172984 !! 1)
         in pow ([head __1172984, _1172996], pow_cont_1173002)
      pow_then_1173032 _ = ret_1173009 1
      _1173045 = bool2bit (0 == (__1172984 !! 1))
   in ([pow_else_1172967, pow_then_1173032] !! _1173045) ()
