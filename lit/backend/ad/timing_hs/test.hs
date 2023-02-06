{-# LANGUAGE BangPatterns #-}
{-# LANGUAGE ViewPatterns #-}
{-# OPTIONS_GHC -Wno-unrecognised-pragmas #-}
{-# LANGUAGE ScopedTypeVariables #-}
{-# HLINT ignore "Use camelCase" #-}

import           Control.DeepSeq
import           Opt
import           Preopt
import           System.CPUTime
import           System.TimeIt
import Control.Monad.IO.Class (MonadIO(liftIO))
import Text.Printf

-- time (def "" -> name) f x = do
--   let start_time = get_time ()
--   let y = f x
--   let end_time = get_time ()
--   let time = end_time - start_time
--   print (name, time)
--   return y

pow_manual :: (Int, Int) -> Int
pow_manual (a, 0) = 1
pow_manual (a, b) = a * pow_manual (a, b - 1)

-- f_diffed_hs :: (Int, Int) -> (Int, (Int, Int))
-- f_diffed_hs (a, b) = (a ^ b, (b * a ^ (b - 1), 0))

tests =
  [ ((i * 37 + 12) `mod` 10000, (i `mod` 100) + 1)
    | i <- [1 .. 100000]
  ]

timeItNamed :: MonadIO m => String -> m a -> m a
timeItNamed name ioa = do
    (t, a) <- timeItT ioa
    liftIO $ printf ("%10s: %6.4fs\n") name t
    return a

test tests name f =
  -- do
  --   start <- getCPUTime
  --   let !r = foo inputList
  --   end <- getCPUTime
  --   let diff = end - start in
  --   print "Execution time: " diff
  --   -- return (end - start)
  Main.timeItNamed name $
    ( -- let !r = (map f tests) in
      -- let !r = sum (map (\t -> let !(a,(b,x)) = f t in a+b+x) tests) in
      -- let !r = sum (map (\t -> let !(a,(b,x)) = f t in x) tests) in
      let r = (map (\t -> f t) tests)
       in r `deepseq` (return ())
      -- print r
    )

wrap f (a, b) =
  let (r, [da, db]) = f ([a, b], id)
   in (r, (da, db))

pow_full_pe :: (Int, Int) -> (Int, (Int, Int))
pow_full_pe (a, b) = (pow_manual (a, b), (b * pow_manual (a, b - 1), 0))

zero_pb :: Int -> (Int, Int)
zero_pb = \_ -> (0, 0)

pair_sum :: (Int, Int) -> (Int, Int) -> (Int, Int)
pair_sum (a, b) (c, d) = (a + c, b + d)

app_pb f (a, a_pb) = let (r, r_pb) = f a in (r, \s -> a_pb (r_pb s))

tup_pb (a, a_pb) (b, b_pb) = ((a, b), \(s1, s2) -> pair_sum (a_pb s1) (b_pb s2))

mul_diffed :: (Int, Int) -> (Int, Int -> (Int, Int))
mul_diffed (a, b) = (a * b, \s -> (b * s, a * s))

sub_diffed :: (Int, Int) -> (Int, Int -> (Int, Int))
sub_diffed (a, b) = (a - b, \s -> (s, -s))

const_diffed :: Int -> (Int, Int -> (Int, Int))
const_diffed c = (c, zero_pb)

pow_hs_r :: (Int, Int) -> (Int, Int -> (Int, Int))
pow_hs_r (a, 0) = const_diffed 1
pow_hs_r (a, b) =
  let a'  = (a, \s -> (s, 0)) in
  let b'  = (b, \s -> (0, s)) in
  let bo' = tup_pb b' (const_diffed 1) in
  let r1' = app_pb sub_diffed bo' in
  let t1' = tup_pb a' r1' in
  let r2' = app_pb pow_hs_r t1' in
  let t2' = tup_pb a' r2' in
  app_pb mul_diffed t2'

pow_hs :: (Int, Int) -> (Int, (Int, Int))
pow_hs arg = let (r, r_pb) = pow_hs_r arg in (r, r_pb 1)

pow_pe_r :: (Int, Int) -> (Int, Int -> (Int, Int))
pow_pe_r (a, 0) = const_diffed 1
pow_pe_r (a, b) =
  let (r, r_pb) = pow_pe_r (a, b - 1) in
  (
    a * r,
    \s ->
      let (da, db) = r_pb s in
      (r+a*da, 0)
  )

pow_pe :: (Int, Int) -> (Int, (Int, Int))
pow_pe arg = let (r, r_pb) = pow_pe_r arg in (r, r_pb 1)

main = do
  test tests "PreOpt" (wrap Preopt.f_diffed)
  test tests "Opt" (wrap Opt.f_diffed)
  test tests "Manual" pow_hs
  test tests "PE" pow_pe
  test tests "FullPE" pow_full_pe

-- print (wrap Opt.f_diffed (42, 2))
