#!/bin/sh
# Run the CPU2006 benchmarks on ARM

HOME=/home/ghassan
ARNAME=ziped_benches.tar.xz

# extract benchmarks
tar xJvf $ARNAME

# bzip2
echo "Running bzip2"
cp 401.bzip2/exe/bzip2_base..exe 401.bzip2/run/run_base_test_.exe.0000/.
cd /home/ghassan/401.bzip2/run/run_base_test_.exe.0000
time /bin/sh -c "../run_base_test_.exe.0000/bzip2_base..exe input.program 5 > input.program.out 2>> input.program.err
../run_base_test_.exe.0000/bzip2_base..exe dryer.jpg 2 > dryer.jpg.out 2>> dryer.jpg.err"
cd $HOME

# namd
echo "Running namd"
cp 444.namd/exe/namd_base..exe 444.namd/run/run_base_test_.exe.0000/.
cd /home/ghassan/444.namd/run/run_base_test_.exe.0000
time /bin/sh -c "../run_base_test_.exe.0000/namd_base..exe --input namd.input --iterations 1 --output namd.out  > namd.stdout 2>> namd.err"
cd $HOME

# gobmk
echo "Running gobmk"
cp 445.gobmk/exe/gobmk_base..exe 445.gobmk/run/run_base_test_.exe.0000/.
cd /home/ghassan/445.gobmk/run/run_base_test_.exe.0000
time /bin/sh -c "../run_base_test_.exe.0000/gobmk_base..exe --quiet --mode gtp < capture.tst > capture.out 2>> capture.err
../run_base_test_.exe.0000/gobmk_base..exe --quiet --mode gtp < connect.tst > connect.out 2>> connect.err
../run_base_test_.exe.0000/gobmk_base..exe --quiet --mode gtp < connect_rot.tst > connect_rot.out 2>> connect_rot.err
../run_base_test_.exe.0000/gobmk_base..exe --quiet --mode gtp < connection.tst > connection.out 2>> connection.err
../run_base_test_.exe.0000/gobmk_base..exe --quiet --mode gtp < connection_rot.tst > connection_rot.out 2>> connection_rot.err
../run_base_test_.exe.0000/gobmk_base..exe --quiet --mode gtp < cutstone.tst > cutstone.out 2>> cutstone.err
../run_base_test_.exe.0000/gobmk_base..exe --quiet --mode gtp < dniwog.tst > dniwog.out 2>> dniwog.err"
cd $HOME

# hmmer
echo "Running hmmer"
cp 456.hmmer/exe/hmmer_base..exe 456.hmmer/run/run_base_test_.exe.0000/.
cd /home/ghassan/456.hmmer/run/run_base_test_.exe.0000
time /bin/sh -c "../run_base_test_.exe.0000/hmmer_base..exe --fixed 0 --mean 325 --num 45000 --sd 200 --seed 0 bombesin.hmm > bombesin.out 2>> bombesin.err"
cd $HOME

# h264ref
echo "Running h264ref"
cp 464.h264ref/exe/h264ref_base..exe 464.h264ref/run/run_base_test_.exe.0000/.
cd /home/ghassan/464.h264ref/run/run_base_test_.exe.0000
time /bin/sh -c "../run_base_test_.exe.0000/h264ref_base..exe -d foreman_test_encoder_baseline.cfg > foreman_test_baseline_encodelog.out 2>> foreman_test_baseline_encodelog.err"
cd $HOME

# lbm
echo "Running lbm"
cp 470.lbm/exe/lbm_base..exe 470.lbm/run/run_base_test_.exe.0000/.
cd /home/ghassan/470.lbm/run/run_base_test_.exe.0000
time /bin/sh -c "../run_base_test_.exe.0000/lbm_base..exe 20 reference.dat 0 1 100_100_130_cf_a.of > lbm.out 2>> lbm.err"
cd $HOME

# sphinx
#echo "Running sphinx"
#cp 482.sphinx3/exe/sphinx_livepretend_base..exe 482.sphinx3/run/run_base_test_.exe.0000/.
#cd /home/ghassan/482.sphinx3/run/run_base_test_.exe.0000
#time /bin/sh -c "../run_base_test_.exe.0000/sphinx_livepretend_base..exe ctlfile . args.an4 > an4.log 2>> an4.err"
#cd $HOME
