{-# LANGUAGE ViewPatterns #-}
{-# LANGUAGE BangPatterns #-}

import Opt
import Preopt
import System.TimeIt
import Control.DeepSeq
import System.CPUTime

-- time (def "" -> name) f x = do
--   let start_time = get_time ()
--   let y = f x
--   let end_time = get_time ()
--   let time = end_time - start_time
--   print (name, time)
--   return y

tests =
  [ ((i * 37 + 12) `mod` 10000, i `mod` 100)
    | i <- [1 .. 100000]
  ]

test tests f = 
  -- do
  --   start <- getCPUTime
  --   let !r = foo inputList
  --   end <- getCPUTime
  --   let diff = end - start in
  --   print "Execution time: " diff
  --   -- return (end - start)
  timeIt $ (
    -- let !r = (map f tests) in
    -- let !r = sum (map (\t -> let !(a,(b,x)) = f t in a+b+x) tests) in
    -- let !r = sum (map (\t -> let !(a,(b,x)) = f t in x) tests) in
    let r = (map (\t -> f t) tests) in
      r `deepseq` (return ())
      -- print r
  )

wrap f (a, b) =
  let (r, [da, db]) = f ([a, b], id)
   in (r, (da, db))

main = do
  test tests (wrap Opt.f_diffed)
  test tests (wrap Preopt.f_diffed)
  print (wrap Opt.f_diffed (42,2))
