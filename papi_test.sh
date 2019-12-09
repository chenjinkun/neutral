ARCH=$1

if [ $ARCH == "kp920" ]
then
#test for ocoe
    for ocoe in {1..16}
    do
        numactl --physcpubind=0 --localalloc ./neutral.omp3.kp920 problems/csp.params 0 $ocoe
    done
    mv result* ../../../../../../PAPI_test_result/kp920/neutral/

#test for mpx 20
    for num in {1..8}
    do
        cp ../../../../../events/kp920/kp920_events_mpx_20_$num.txt ../../../../../events/kp920/kp920_events_mpx.txt
        numactl --physcpubind=0 --localalloc ./neutral.omp3.kp920 problems/csp.params 0 0 
        mv result_neutral_mpx.csv ../../../../../../PAPI_test_result/kp920/neutral/result_neutral_mpx_20_$num.csv 
    done

#test for mpx 40
    for num in {1..4}
    do
        cp ../../../../../events/kp920/kp920_events_mpx_40_$num.txt ../../../../../events/kp920/kp920_events_mpx.txt
        numactl --physcpubind=0 --localalloc ./neutral.omp3.kp920 problems/csp.params 0 0 
        mv result_neutral_mpx.csv ../../../../../../PAPI_test_result/kp920/neutral/result_neutral_mpx_40_$num.csv 
    done

#test for mpx 80
    for num in {1..2}
    do
        cp ../../../../../events/kp920/kp920_events_mpx_80_$num.txt ../../../../../events/kp920/kp920_events_mpx.txt
        numactl --physcpubind=0 --localalloc ./neutral.omp3.kp920 problems/csp.params 0 0 
        mv result_neutral_mpx.csv ../../../../../../PAPI_test_result/kp920/neutral/result_neutral_mpx_80_$num.csv
    done

#test for mpx 155
    cp ../../../../../events/kp920/kp920_events_mpx_155.txt ../../../../../events/kp920/kp920_events_mpx.txt
    numactl --physcpubind=0 --localalloc ./neutral.omp3.kp920 problems/csp.params 0 0 
    mv result_neutral_mpx.csv ../../../../../../PAPI_test_result/kp920/neutral/result_neutral_mpx_155.csv 
    
    rm ../../../../../events/kp920/kp920_events_mpx.txt
elif [ $ARCH == "hsw" ]
then
#test for ocoe
    for ocoe in {1..41}
    do
        numactl --physcpubind=0 --localalloc ./neutral.omp3.hsw problems/csp.params 1 $ocoe
    done
    mv result* ../../../../../../PAPI_test_result/hsw/neutral/

#test for mpx 16
    for num in {1..21}
    do
        cp ../../../../../events/hsw/hsw_events_mpx_16_$num.txt ../../../../../events/hsw/hsw_events_mpx.txt
        numactl --physcpubind=0 --localalloc ./neutral.omp3.hsw problems/csp.params 1 0 
        mv result_neutral_mpx.csv ../../../../../../PAPI_test_result/hsw/neutral/result_neutral_mpx_16_$num.csv 
    done

#test for mpx 20
    for num in {1..17}
    do
        cp ../../../../../events/hsw/hsw_events_mpx_20_$num.txt ../../../../../events/hsw/hsw_events_mpx.txt
        numactl --physcpubind=0 --localalloc ./neutral.omp3.hsw problems/csp.params 1 0 
        mv result_neutral_mpx.csv ../../../../../../PAPI_test_result/hsw/neutral/result_neutral_mpx_20_$num.csv 
    done

#test for mpx 40
    for num in {1..9}
    do
        cp ../../../../../events/hsw/hsw_events_mpx_40_$num.txt ../../../../../events/hsw/hsw_events_mpx.txt
        numactl --physcpubind=0 --localalloc ./neutral.omp3.hsw problems/csp.params 1 0 
        mv result_neutral_mpx.csv ../../../../../../PAPI_test_result/hsw/neutral/result_neutral_mpx_40_$num.csv 
    done

#test for mpx 80
    for num in {1..5}
    do
        cp ../../../../../events/hsw/hsw_events_mpx_80_$num.txt ../../../../../events/hsw/hsw_events_mpx.txt
        numactl --physcpubind=0 --localalloc ./neutral.omp3.hsw problems/csp.params 1 0 
        mv result_neutral_mpx.csv ../../../../../../PAPI_test_result/hsw/neutral/result_neutral_mpx_80_$num.csv 
    done

#test for mpx 160
    for num in {1..3}
    do
        cp ../../../../../events/hsw/hsw_events_mpx_160_$num.txt ../../../../../events/hsw/hsw_events_mpx.txt
        numactl --physcpubind=0 --localalloc ./neutral.omp3.hsw problems/csp.params 1 0 
        mv result_neutral_mpx.csv ../../../../../../PAPI_test_result/hsw/neutral/result_neutral_mpx_160_$num.csv 
    done

#test for mpx 328
    cp ../../../../../events/hsw/hsw_events_mpx_328.txt ../../../../../events/hsw/hsw_events_mpx.txt
    numactl --physcpubind=0 --localalloc ./neutral.omp3.hsw problems/csp.params 1 0 
    mv result_neutral_mpx.csv ../../../../../../PAPI_test_result/hsw/neutral/result_neutral_mpx_328.csv 
    
    rm ../../../../../events/hsw/hsw_events_mpx.txt
fi