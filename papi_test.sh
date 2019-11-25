ARCH=$1

if [ $ARCH == "kp920" ]
then
#test for ocoe
    for ocoe in {1..16}
    do
        numactl --physcpubind=0 --localalloc ./neutral.omp3.kp920 problems/csp.params 0 $ocoe
    done
    mv result* ../../../../../result/kp920/neutral/

#test for mpx 20
    for num in {1..7}
    do
        cp ../../../../../events/kp920/kp920_events_mpx_20_$num.txt ../../../../../events/kp920/kp920_events_mpx.txt
        numactl --physcpubind=0 --localalloc ./neutral.omp3.kp920 problems/csp.params 0 0 
        mv result_neutral_mpx.csv ../../../../../result/kp920/neutral/result_neutral_mpx_20_$num.csv 
    done

#test for mpx 40
    for num in {1..3}
    do
        cp ../../../../../events/kp920/kp920_events_mpx_40_$num.txt ../../../../../events/kp920/kp920_events_mpx.txt
        numactl --physcpubind=0 --localalloc ./neutral.omp3.kp920 problems/csp.params 0 0 
        mv result_neutral_mpx.csv ../../../../../result/kp920/neutral/result_neutral_mpx_40_$num.csv 
    done

#test for mpx 80
    cp ../../../../../events/kp920/kp920_events_mpx_80.txt ../../../../../events/kp920/kp920_events_mpx.txt
    numactl --physcpubind=0 --localalloc ./neutral.omp3.kp920 problems/csp.params 0 0 
    mv result_neutral_mpx.csv ../../../../../result/kp920/neutral/result_neutral_mpx_80.csv

#test for mpx 155
    cp ../../../../../events/kp920/kp920_events_mpx_155.txt ../../../../../events/kp920/kp920_events_mpx.txt
    numactl --physcpubind=0 --localalloc ./neutral.omp3.kp920 problems/csp.params 0 0 
    mv result_neutral_mpx.csv ../../../../../result/kp920/neutral/result_neutral_mpx_155.csv 
    
    rm ../../../../../events/kp920/kp920_events_mpx.txt 
fi