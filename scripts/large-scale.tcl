set ns [new Simulator]

set argc_ok [expr $argc >= 4]; ###!! set argc_ok 1
if {[expr !$argc_ok]} { 
  puts "USAGE: ./ns scripts/large-scale.tcl {ccc_type} {experiment_id} {DEPLOY_STEP} {traffic_locality}"       
  puts "DEPLOY_STEP: 0, 25, 50, 75, 100" ;                                                                              
  puts "traffic_locality: 0 - 100 (in percent)"                                                                         
  exit 1                                                                                                                
}                                                                                                                       
# Configurations
set linkRate 40 ;# Gb
set linkRateHigh 100 ;#Gb
set linkRateBw 40Gb
set linkRateHighBw 100Gb
set hostDelay 0.000002
set linkDelayHostTor 0.000004
set linkDelayTorAggr 0.000004
set linkDelayAggrCore 0.000004
set creditQueueCapacity [expr 84*10] ;# Bytes
set maxCreditBurst [expr 84*2]
set creditRate [expr 64734895*4] ;# bytes/sec
set creditRateHigh [expr 64734895*10] ;# bytes/sec for 40Gbps link
set baseCreditRate [expr 64734895*4] ;# bytes/sec
set K 65
set K_xpass 25; #[expr int([lindex $argv 3])]
set KHigh [expr $K*2.5]
set B [expr 1000] 
set BHigh [expr $B*2.5]
set B_host 1000
set dataQueueCapacity [expr $B * 1538] ;# bytes (shared buffer data capacity)
set numFlow 10000; #10000                                                 ###!! set numFlow <<<numFlow>>>
set workload "cachefollower" ;# cachefollower, mining, search, webserver  ###!! set workload "<<<workload>>>"
set linkLoad 0.6 ;# ranges from 0.0 to 1.0                    ###!! set linkLoad "<<<linkLoad>>>"
set expID [expr int([lindex $argv 1])];                       ###!! set expID [expr int(<<<expID>>>)];
set deployStep [expr int(int([lindex $argv 2])/25)];          ###!! set deployStep [expr int(int(<<<deployStep>>>)/25)];
set trafficLocality [expr int([lindex $argv 3])];             ###!! set trafficLocality [expr int(<<<trafficLocality>>>)];
set enable_queue_trace 0
set packetSize 1460

# xpass_queue_weight: Configurable parameter of the weight of XPass queue, assuming there is no credit queue. Valid value: (0, 1)
# This value should be applied to DWRR queues (DWRR::set-quantum) (since they don't consider credit queue when doing DWRR)
set xpass_queue_weight 0.5                                      ; ###!! set xpass_queue_weight <<<xpassQueueWeight>>>
set queue_quantum_default 1538
set actual_xpass_queue_weight 0.5; #will be recalculated

set ccc_type [lindex $argv 0]; # xpass, gdx, flexpass  ###!! set ccc_type <<<cccType>>>
set flexpassSelDropThresh 100000; #on flexpass, will be updated later
set flexpassScheme 1; #on flexpass, will be updated later               

set deploy_per_cluster 1; # if 0, deployment would be random     ###!! set deploy_per_cluster <<<deployPerCluster>>>
set foreground_flow_ratio 0.05                                  ;###!! set foreground_flow_ratio <<<foregroundFlowRatio>>>
set foreground_flow_size 8000                                   ;###!! set foreground_flow_size <<<foregroundFlowSize>>>
set enableSharedBuffer 1                                        ;###!! set enableSharedBuffer <<<enableSharedBuffer>>>
set newFlexPassAllocationLogic 1                                ;###!! set newFlexPassAllocationLogic <<<newFlexPassAllocationLogic>>>

set tcpInitWindow 5                                             ;###!! set tcpInitWindow <<<tcpInitWindow>>>
set flexpassReactiveInitWindow 5                                ;###!! set flexpassReactiveInitWindow <<<flexpassReactiveInitWindow>>>


set reordering_measure_in_rc3_ 0;                               ;###!! set reordering_measure_in_rc3_ <<<reorderingMeasureInRc3>>>
set static_allocation_ 0;                                       ;###!! set static_allocation_ <<<staticAllocation>>>
set xpass_queue_isolation_ 0;                                   ;###!! set xpass_queue_isolation_ <<<xpassQueueIsolation>>>
set strict_high_priority_ 0;                                    ;###!! set strict_high_priority_ <<<strictHighPriority>>>
set numFlowPerHost 4;                                           ;###!! set numFlowPerHost <<<numForegroundFlowPerHost>>>
set oracleQueueWeight 0;                                        ;###!! set oracleQueueWeight <<<oracleQueueWeight>>>

if  {$enableSharedBuffer} {
  set B [expr 340000] ; # set to infinity. Approx. 2^29 Bytes. RED queue itself should not drop any packets! # [expr 250] 
  set BHigh [expr $B*2.5]
  set dataQueueCapacity [expr $B * 1538] ;# bytes (shared buffer data capacity)
}

if {!$newFlexPassAllocationLogic} {
  puts "Caution: Using legacy allocation logic!"
}

if {([string compare $ccc_type "xpass"] == 0)} {
  # set enable_dwrr_q 0;
  set enable_dwrr_q 1;
  set enable_tos_ $xpass_queue_isolation_;
  set enable_non_xpass_selective_dropping_ 0;
} elseif {([string compare $ccc_type "gdx"] == 0)} {
  set enable_dwrr_q 1;
  set enable_tos_ 0;
  set enable_non_xpass_selective_dropping_ 0;
} elseif {([string compare $ccc_type "flexpass"] == 0)} {
  set enable_dwrr_q 1;
  set enable_tos_ 1;
  set enable_non_xpass_selective_dropping_ 1;
  set flexpassScheme  [expr int([lindex $argv 4])]             ; ###!! set flexpassScheme <<<flexpassScheme>>>
  set flexpassSelDropThresh [expr int([lindex $argv 5])]       ; ###!! set flexpassSelDropThresh <<<flexpassSelDropThresh>>>

} else {
  puts "Invalid Credit-based CC type: must be one of xpass, gdx, flexpass"
  exit 1
}

if {[string compare $ccc_type "flexpass"] == 0} {
    # set xpass_queue_weight_ratio 0;
    # set actual_xpass_queue_weight 0.5;
    
    # xpass_queue_weight_ratio: ratio of XPass queue weight compared to TCP queue weight
    # set xpass_queue_weight_ratio [expr $xpass_queue_weight / (1 - $xpass_queue_weight)] 
    # actual_xpass_queue_weight: actual weight of XPass queue, considering credit queue
    # This value should be applied to credit generators (Agents::flexpass_beta_) and credit rate limiters (DWRR::token_refresh_rate_)
    set actual_xpass_queue_weight [expr (1538.0-84.0)/ 1538 * $xpass_queue_weight];
    
    puts "xpass_queue_weight = ${xpass_queue_weight}"
    puts "actual_xpass_queue_weight = ${actual_xpass_queue_weight}"
    
    set creditRate [expr $creditRate * $actual_xpass_queue_weight];
    set baseCreditRate [expr $baseCreditRate * $actual_xpass_queue_weight];
    set creditRateHigh [expr $creditRateHigh * $actual_xpass_queue_weight];
} elseif {[string compare $ccc_type "xpass"] == 0} {
  if {$oracleQueueWeight && $xpass_queue_isolation_ && [expr $deployStep > 0]} {
    
     # if {$xpass_queue_isolation_ && !$strict_high_priority_ && [expr $deployStep > 0]}
    # automatically adjust weight 
    set creditRate [expr $creditRate * ($deployStep/4.0)];
    set baseCreditRate [expr $baseCreditRate* ($deployStep/4.0)];
    set creditRateHigh [expr $creditRateHigh * ($deployStep/4.0)];
    set xpass_queue_weight [expr ($deployStep/4.0)];
    if {$xpass_queue_weight == 1} {
      puts "Assuming full deployment. not changing queue weight."
      set xpass_queue_weight 0.5;
    }
    puts "Baseline with DWRR XPass. Automatically changing credit rate to ${creditRate}"
  } elseif {$xpass_queue_isolation_ && [expr $deployStep > 0]} {
    set creditRate [expr $creditRate * 0.5];
    set baseCreditRate [expr $baseCreditRate * 0.5];
    set creditRateHigh [expr $creditRateHigh * 0.5];
    set xpass_queue_weight 0.5;
    puts "Baseline with DWRR XPass. This is not oracle. changing credit rate to ${creditRate}"
  }
}

puts "expID = $expID"
puts "deployStep = $deployStep"
puts "traffic_locality = ${trafficLocality}%"

if {$deploy_per_cluster > 0} {
  puts "Deployment Strategy : Per Cluster"
} else {
  puts "Deployment Strategy : Per Service (Random)"
}
# Toplogy configurations
set dataBufferHost [expr 1000*1538] ;# bytes / port
set dataBufferFromTorToAggr [expr $B*1538] ;# bytes / port
set dataBufferFromAggrToCore [expr $B*1538] ;# bytes / port
set dataBufferFromCoreToAggr [expr $B*1538] ;# bytes / port
set dataBufferFromAggrToTor [expr $B*1538] ;# bytes / port
set dataBufferFromTorToHost [expr $B*1538] ;# bytes / port

set numCore 8 ;# number of core switches
set numAggr 16 ;# number of aggregator switches
set numTor 32 ;# number of ToR switches
set numNode 192 ;# number of nodes

# XPass configurations
set alpha 0.5; #0.0625
set w_init 0.0625
set minJitter -0.1
set maxJitter 0.1
set minEthernetSize 78
set maxEthernetSize 1538
set minCreditSize 78
set maxCreditSize 90
set xpassHdrSize 78
set maxPayload [expr $maxEthernetSize-$xpassHdrSize]
set avgCreditSize [expr ($minCreditSize+$maxCreditSize)/2.0]
set creditBW [expr $linkRate*125000000*$avgCreditSize/($avgCreditSize+$maxEthernetSize)]
set creditBW [expr int($creditBW)]

# Simulation setup
set simStartTime 0.1
set simEndTime 4


# Output file
file mkdir "outputs"
set nt [open "outputs/trace_$expID.out" w]
set fct_out [open "outputs/fct_$expID.out" w]
set wst_out [open "outputs/waste_$expID.out" w]
# set nam [open "outputs/trace_$expID.nam" w]

set tot_qlenfile [open outputs/tot_qlenfile_$expID.tr w]
set qlenfile [open outputs/qlenfile_$expID.tr w]

puts $fct_out "Flow ID,Flow Size (bytes),Flow Completion Time (secs)"
puts $wst_out "Flow ID,Flow Size (bytes),Wasted Credit"
close $fct_out
close $wst_out

set flowfile [open outputs/flowfile_$expID.tr w]

set vt [open outputs/var_$expID.out w]

# $ns namtrace-all $nam

proc finish {} {
  global ns nt vt sender N flowfile 
  #nam
  $ns flush-trace 
  close $nt
  close $flowfile
  # close $nam
  puts "Simulation terminated successfully."
  exit 0
}
#$ns trace-all $nt

DelayLink set avoidReordering_ true
# $ns rtproto DV
# Agent/rtProto/DV set advertInterval 10
Node set multiPath_ 1
Classifier/MultiPath set symmetric_ true
Classifier/MultiPath set nodetype_ 0

# Workloads setting
if {[string compare $workload "mining"] == 0} {
  set workloadPath "workloads/workload_mining.tcl"
  set avgFlowSize 7410212
} elseif {[string compare $workload "search"] == 0} {
  set workloadPath "workloads/workload_search.tcl"
  set avgFlowSize 1654275
} elseif {[string compare $workload "cachefollower"] == 0} {
  set workloadPath "workloads/workload_cachefollower.tcl"
  set avgFlowSize 701490
} elseif {[string compare $workload "webserver"] == 0} {
  set workloadPath "workloads/workload_webserver.tcl"
  set avgFlowSize 63735
} elseif {[string compare $workload "me"] == 0} {
  set workloadPath "workloads/workload_me.tcl"
  set avgFlowSize 1090000 
} elseif {[string compare $workload "google"] == 0} {
  set workloadPath "workloads/workload_google.tcl"
  set avgFlowSize 71110 
} elseif {[string compare $workload "facebook"] == 0} {
  set workloadPath "workloads/workload_facebook.tcl"
  set avgFlowSize 504292 
} else {
  puts "Invalid workload: $workload"
  exit 0
}

set linkLoadForeground [expr $linkLoad * $foreground_flow_ratio];
set linkLoadBackground [expr $linkLoad * (1-$foreground_flow_ratio)];

puts "Total Load: $linkLoad, BG Load: $linkLoadBackground, FG Load: $linkLoadForeground"

set overSubscRatio [expr double($numNode/$numTor)/double($numTor/$numAggr)]
#set lambda [expr ($numNode*$linkRate*1000000000*$linkLoad)/($avgFlowSize*8.0/$maxPayload*$maxEthernetSize)]
set lambda [expr ($numNode*$linkRate*1000000000*$linkLoadBackground)/($avgFlowSize*8.0/$maxPayload*$maxEthernetSize)]
set foreground_lambda [expr ($numNode*$linkRate*1000000000*$linkLoadForeground)/($foreground_flow_size*$numNode*8.0/$maxPayload*$maxEthernetSize)]
set avgFlowInterval [expr $overSubscRatio/$lambda]
if {$foreground_flow_ratio > 0} {
  set foregroundFlowInterval [expr $overSubscRatio/$foreground_lambda];
}

# Random number generators
set RNGFlowSize [new RNG]
$RNGFlowSize seed 61569011

set RNGFlowInterval [new RNG]
$RNGFlowInterval seed 94762103

set RNGSrcNodeId [new RNG]
$RNGSrcNodeId seed 17391005

set RNGDstNodeId [new RNG]
$RNGDstNodeId seed 35010256

set RNGDeterLocal [new RNG]
$RNGDeterLocal seed 98143256

set RNGLocalOffset [new RNG]
$RNGLocalOffset seed 1928397

set RNGDeterDeployed [new RNG]
$RNGDeterDeployed seed 518912

set randomFlowSize [new RandomVariable/Empirical]
$randomFlowSize use-rng $RNGFlowSize
$randomFlowSize set interpolation_ 2
$randomFlowSize loadCDF $workloadPath

set randomFlowInterval [new RandomVariable/Exponential]
$randomFlowInterval use-rng $RNGFlowInterval
$randomFlowInterval set avg_ $avgFlowInterval

set randomSrcNodeId [new RandomVariable/Uniform]
$randomSrcNodeId use-rng $RNGSrcNodeId
$randomSrcNodeId set min_ 0
$randomSrcNodeId set max_ $numNode

set randomDstNodeId [new RandomVariable/Uniform]
$randomDstNodeId use-rng $RNGDstNodeId
$randomDstNodeId set min_ 0
$randomDstNodeId set max_ $numNode

set randomDeterLocal [new RandomVariable/Uniform]
$randomDeterLocal use-rng $RNGDeterLocal
$randomDeterLocal set min_ 0
$randomDeterLocal set max_ 100

set numNodeInCluster 24
set randomLocalOffset [new RandomVariable/Uniform]
$randomLocalOffset use-rng $RNGLocalOffset
$randomLocalOffset set min_ 0
$randomLocalOffset set max_ $numNodeInCluster

set randomDeployment [new RandomVariable/Uniform]
$randomDeployment use-rng $RNGDeterDeployed
$randomDeployment set min_ 0
$randomDeployment set max_ 1

Queue/BroadcomNode set selective_dropping_threshold_ $flexpassSelDropThresh
# Node
puts "Creating nodes..."
for {set i 0} {$i < $numNode} {incr i} {
  set dcNode($i) [$ns node]
  $dcNode($i) set nodetype_ 1
}
for {set i 0} {$i < $numTor} {incr i} {
  set dcTor($i) [$ns node]
  $dcTor($i) set nodetype_ 2
  set bcmTor($i) [$ns broadcom]
}
for {set i 0} {$i < $numAggr} {incr i} {
  set dcAggr($i) [$ns node]
  $dcAggr($i) set nodetype_ 3
  set bcmAggr($i) [$ns broadcom]
}
for {set i 0} {$i < $numCore} {incr i} {
  set dcCore($i) [$ns node]
  $dcCore($i) set nodetype_ 4
  set bcmCore($i) [$ns broadcom]
}

# Link
puts "Creating Links..."
Queue/DropTail set mean_pktsize_ 1538
Queue/DropTail set limit_ $B_host

#RED Setting
if {$enable_dwrr_q} {
  set K_port $K
    Queue/DwrrXPassRED set queue_num_ 3
    Queue/DwrrXPassRED set mean_pktsize_ 1538
    
    Queue/DwrrXPassRED set thresh_ $K_port
    Queue/DwrrXPassRED set maxthresh_ $K_port
    Queue/DwrrXPassRED set mark_p_ 2.0
    Queue/DwrrXPassRED set port_thresh_ $K_port
    
    Queue/DwrrXPassRED set marking_scheme_ 0; # Use per-queue ECN
    Queue/DwrrXPassRED set estimate_round_alpha_ 0.75
    Queue/DwrrXPassRED set estimate_round_idle_interval_bytes_ 1500
    
    
    Queue/DwrrXPassRED set link_capacity_ $linkRateBw
    Queue/DwrrXPassRED set deque_marking_ false
    Queue/DwrrXPassRED set debug_ false
    Queue/DwrrXPassRED set credit_limit_ $creditQueueCapacity
    Queue/DwrrXPassRED set data_limit_ $dataQueueCapacity; # assume shared buffer
    Queue/DwrrXPassRED set queue_in_bytes_ true;
    Queue/DwrrXPassRED set limit_ [expr $dataQueueCapacity / 1538];

    
    Queue/DwrrXPassRED set token_refresh_rate_ [expr $creditRate]
    puts "creditRate = $creditRate"
    Queue/DwrrXPassRED set max_tokens_ [expr 2*84] 
    Queue/DwrrXPassRED set exp_id_ $expID
    Queue/DwrrXPassRED set qidx_ 1
    
    Queue/DwrrXPassRED set bytes_ false
    Queue/DwrrXPassRED set setbit_ true
    Queue/DwrrXPassRED set gentle_ false
    Queue/DwrrXPassRED set q_weight_ 1.0

    Queue/DwrrXPassRED set enable_tos_ $enable_tos_
    Queue/DwrrXPassRED set enable_non_xpass_selective_dropping_ $enable_non_xpass_selective_dropping_
    
    Queue/DwrrXPassRED set enable_shared_buffer_ $enableSharedBuffer
    Queue/DwrrXPassRED set selective_dropping_threshold_ $flexpassSelDropThresh
    Queue/DwrrXPassRED set flexpass_queue_scheme_ $flexpassScheme
    Queue/DwrrXPassRED set strict_high_priority_ $strict_high_priority_

} else {
    Queue/XPassRED set queue_num_ 3
    Queue/XPassRED set mean_pktsize_ 1538
    
    Queue/XPassRED set thresh_ $K_port
    Queue/XPassRED set maxthresh_ $K_port
    Queue/XPassRED set mark_p_ 2.0
    Queue/XPassRED set port_thresh_ $K_port

    Queue/XPassRED set marking_scheme_ 0; # Use per-queue ECN
    Queue/XPassRED set estimate_round_alpha_ 0.75
    Queue/XPassRED set estimate_round_idle_interval_bytes_ 1500

    Queue/XPassRED set link_capacity_ $linkRateBw
    Queue/XPassRED set deque_marking_ false
    Queue/XPassRED set debug_ false
    Queue/XPassRED set credit_limit_ $creditQueueCapacity
    Queue/XPassRED set data_limit_ $dataQueueCapacity; # no use
    Queue/XPassRED set queue_in_bytes_ true;
    Queue/XPassRED set limit_ [expr $dataQueueCapacity / 1538];

    Queue/XPassRED set token_refresh_rate_ [expr $creditRate]
    Queue/XPassRED set max_tokens_ [expr 2*84] 
    Queue/XPassRED set exp_id_ $expID
    Queue/XPassRED set qidx_ 1

    Queue/XPassRED set bytes_ false
    Queue/XPassRED set setbit_ true
    Queue/XPassRED set gentle_ false
    Queue/XPassRED set q_weight_ 1.0
}

for {set i 0} {$i < $numAggr} {incr i} {
  set coreIndex [expr $i%2]
  for {set j $coreIndex} {$j < $numCore} {incr j 2} {
    if {$enable_dwrr_q} {
      $ns simplex-link $dcAggr($i) $dcCore($j) [set linkRateHigh]Gb $linkDelayAggrCore DwrrXPassRED
      set link_aggr_core [$ns link $dcAggr($i) $dcCore($j)]
      set queue_aggr_core [$link_aggr_core queue]
      $queue_aggr_core set data_limit_ [expr $dataBufferFromAggrToCore/2]
      $queue_aggr_core set thresh_ $KHigh
      $queue_aggr_core set maxthresh_ $KHigh
      $queue_aggr_core set limit_ $BHigh
      $queue_aggr_core set token_refresh_rate_ [expr $creditRateHigh]
      # guarantee 33% 33% 33%
      $queue_aggr_core set-quantum 0 $queue_quantum_default
      $queue_aggr_core set-quantum 1 [expr $queue_quantum_default*2*$xpass_queue_weight]
      $queue_aggr_core set-quantum 2 [expr $queue_quantum_default*2*(1-$xpass_queue_weight)]
      $queue_aggr_core set-thresh 1 [expr $K_xpass * 4]
      $queue_aggr_core set-thresh 2 $KHigh
      $queue_aggr_core set trace_ 0
      $queue_aggr_core set link_capacity_ $linkRateHighBw
      $queue_aggr_core ingress-node $bcmCore($j)
      $queue_aggr_core egress-node $bcmAggr($i)

      $ns simplex-link $dcCore($j) $dcAggr($i) [set linkRateHigh]Gb $linkDelayAggrCore DwrrXPassRED
      set link_core_aggr [$ns link $dcCore($j) $dcAggr($i)]
      set queue_core_aggr [$link_core_aggr queue]
      $queue_core_aggr set data_limit_ [expr $dataBufferFromCoreToAggr/2]
      $queue_core_aggr set thresh_ $KHigh
      $queue_core_aggr set maxthresh_ $KHigh
      $queue_core_aggr set limit_ $BHigh
      $queue_core_aggr set token_refresh_rate_ [expr $creditRateHigh]
      # guarantee 33% 33% 33%
      $queue_core_aggr set-quantum 0 $queue_quantum_default
      $queue_core_aggr set-quantum 1 [expr $queue_quantum_default*2*$xpass_queue_weight]
      $queue_core_aggr set-quantum 2 [expr $queue_quantum_default*2*(1-$xpass_queue_weight)]
      $queue_core_aggr set-thresh 1 [expr $K_xpass * 4]
      $queue_core_aggr set-thresh 2 $KHigh
      $queue_core_aggr set trace_ 0
      $queue_core_aggr set link_capacity_ $linkRateHighBw
      $queue_core_aggr ingress-node $bcmAggr($i)
      $queue_core_aggr egress-node $bcmCore($j)
    } else {
      $ns simplex-link $dcAggr($i) $dcCore($j) [set linkRateHigh]Gb $linkDelayAggrCore XPassRED
      set link_aggr_core [$ns link $dcAggr($i) $dcCore($j)]
      set queue_aggr_core [$link_aggr_core queue]
      $queue_aggr_core set data_limit_ $dataBufferFromAggrToCore
      $queue_aggr_core set thresh_ $KHigh
      $queue_aggr_core set maxthresh_ $KHigh
      $queue_aggr_core set limit_ $BHigh
      $queue_aggr_core set token_refresh_rate_ $creditRateHigh

      $ns simplex-link $dcCore($j) $dcAggr($i) [set linkRateHigh]Gb $linkDelayAggrCore XPassRED
      set link_core_aggr [$ns link $dcCore($j) $dcAggr($i)]
      set queue_core_aggr [$link_core_aggr queue]
      $queue_core_aggr set data_limit_ $dataBufferFromCoreToAggr
      $queue_core_aggr set thresh_ $KHigh
      $queue_core_aggr set maxthresh_ $KHigh
      $queue_core_aggr set limit_ $BHigh
      $queue_core_aggr set token_refresh_rate_ $creditRateHigh
    }
  }
}

set traceQueueCnt 0

for {set i 0} {$i < $numTor} {incr i} {
  set aggrIndex [expr $i/4*2]
  for {set j $aggrIndex} {$j <= $aggrIndex+1} {incr j} {
    set cLinkRate $linkRate
    set cK $K
    set cB $B
    set cCreditRate $creditRate
    # if {([expr $i/4] == 0) || ([expr $i/4] == 2)} {
    #   set cLinkRate $linkRateHigh
    #   set cK $KHigh
    #   set cB $BHigh
    #   set cCreditRate $creditRateHigh
    # }
    if {$enable_dwrr_q} {
      $ns simplex-link $dcTor($i) $dcAggr($j) [set cLinkRate]Gb $linkDelayTorAggr DwrrXPassRED
      set link_tor_aggr [$ns link $dcTor($i) $dcAggr($j)]
      set queue_tor_aggr [$link_tor_aggr queue]
      $queue_tor_aggr set data_limit_ [expr $dataBufferFromTorToAggr/2]
      $queue_tor_aggr set thresh_ $cK
      $queue_tor_aggr set maxthresh_ $cK
      $queue_tor_aggr set limit_ $cB
      $queue_tor_aggr set token_refresh_rate_ [expr $cCreditRate]
      
      # guarantee 33% 33% 33%
      $queue_tor_aggr set-quantum 0 $queue_quantum_default
      $queue_tor_aggr set-quantum 1 [expr $queue_quantum_default*2*$xpass_queue_weight]
      $queue_tor_aggr set-quantum 2 [expr $queue_quantum_default*2*(1-$xpass_queue_weight)]
      $queue_tor_aggr set-thresh 1 $K_xpass
      $queue_tor_aggr set-thresh 2 $K_port
      $queue_tor_aggr set exp_id_ $expID
      $queue_tor_aggr ingress-node $bcmAggr($j)
      $queue_tor_aggr egress-node $bcmTor($i)
      $ns simplex-link $dcAggr($j) $dcTor($i) [set cLinkRate]Gb $linkDelayTorAggr DwrrXPassRED
      set link_aggr_tor [$ns link $dcAggr($j) $dcTor($i)]
      set queue_aggr_tor [$link_aggr_tor queue]
      $queue_aggr_tor set data_limit_ [expr $dataBufferFromAggrToTor/2]
      $queue_aggr_tor set thresh_ $cK
      $queue_aggr_tor set maxthresh_ $cK
      $queue_aggr_tor set limit_ $cB
      $queue_aggr_tor set token_refresh_rate_ [expr $cCreditRate]
      
      # guarantee 33% 33% 33%
      $queue_aggr_tor set-quantum 0 $queue_quantum_default
      $queue_aggr_tor set-quantum 1 [expr $queue_quantum_default*2*$xpass_queue_weight]
      $queue_aggr_tor set-quantum 2 [expr $queue_quantum_default*2*(1-$xpass_queue_weight)]
      $queue_aggr_tor set-thresh 1 $K_xpass
      $queue_aggr_tor set-thresh 2 $K_port
      $queue_aggr_tor ingress-node $bcmTor($i)
      $queue_aggr_tor egress-node $bcmAggr($j)
    } else {
      $ns simplex-link $dcTor($i) $dcAggr($j) [set cLinkRate]Gb $linkDelayTorAggr XPassRED
      set link_tor_aggr [$ns link $dcTor($i) $dcAggr($j)]
      set queue_tor_aggr [$link_tor_aggr queue]
      $queue_tor_aggr set data_limit_ $dataBufferFromTorToAggr
      $queue_tor_aggr set thresh_ $cK
      $queue_tor_aggr set maxthresh_ $cK
      $queue_tor_aggr set limit_ $cB
      $queue_tor_aggr set token_refresh_rate_ $cCreditRate
      if {$enable_queue_trace} {
        $queue_tor_aggr set trace_ 1
        $queue_tor_aggr set qidx_ $traceQueueCnt
        $queue_tor_aggr set exp_id_ $expID
        set ql_out [open "outputs/queue_exp${expID}_$traceQueueCnt.tr" w]
        puts $ql_out "Now, Qavg, Qmax"
        close $ql_out
        set traceQueueCnt [expr $traceQueueCnt + 1]
      }

      $ns simplex-link $dcAggr($j) $dcTor($i) [set cLinkRate]Gb $linkDelayTorAggr XPassRED
      set link_aggr_tor [$ns link $dcAggr($j) $dcTor($i)]
      set queue_aggr_tor [$link_aggr_tor queue]
      $queue_aggr_tor set data_limit_ $dataBufferFromAggrToTor
      $queue_aggr_tor set thresh_ $cK
      $queue_aggr_tor set maxthresh_ $cK
      $queue_aggr_tor set limit_ $cB
      $queue_aggr_tor set token_refresh_rate_ $cCreditRate
      if {$enable_queue_trace} {
        $queue_aggr_tor set trace_ 1
        $queue_aggr_tor set qidx_ $traceQueueCnt
        $queue_aggr_tor set exp_id_ $expID
        set ql_out [open "outputs/queue_exp${expID}_$traceQueueCnt.tr" w]
        puts $ql_out "Now, Qavg, Qmax"
        close $ql_out
        set traceQueueCnt [expr $traceQueueCnt + 1]
      }
    }
  }
}

for {set i 0} {$i < $numNode} {incr i} {
  set torIndex [expr $i/($numNode/$numTor)]
  set cLinkRate $linkRate
  set cK $K
  set cB $B
  set cCreditRate $creditRate
  # if {([expr $torIndex/4] == 0) || ([expr $torIndex/4] == 2)} {
  #   set cLinkRate $linkRateHigh
  #   set cK $KHigh
  #   set cB $BHigh
  #   set cCreditRate $creditRateHigh
  # }

  if {$enable_dwrr_q} {    
    $ns simplex-link $dcNode($i) $dcTor($torIndex) [set cLinkRate]Gb [expr $linkDelayHostTor+$hostDelay] DwrrXPassRED
    set link_host_tor [$ns link $dcNode($i) $dcTor($torIndex)]
    set queue_host_tor [$link_host_tor queue]
    
    $queue_host_tor set data_limit_ [expr $dataBufferFromTorToHost/2]
    $queue_host_tor set thresh_ $cK
    $queue_host_tor set maxthresh_ $cK
    $queue_host_tor set limit_ $cB
    $queue_host_tor set limit_ $cB
    # guarantee 33% 33% 33%
    $queue_host_tor set-quantum 0 $queue_quantum_default
    $queue_host_tor set-quantum 1 [expr $queue_quantum_default*2*$xpass_queue_weight]
    $queue_host_tor set-quantum 2 [expr $queue_quantum_default*2*(1-$xpass_queue_weight)]
    $queue_host_tor set-thresh 1 $K_xpass
    $queue_host_tor set-thresh 2 $K_port
    $queue_host_tor ingress-node $bcmTor($torIndex)


    $ns simplex-link $dcTor($torIndex) $dcNode($i) [set cLinkRate]Gb $linkDelayHostTor DwrrXPassRED
    set link_tor_host [$ns link $dcTor($torIndex) $dcNode($i)]
    set queue_tor_host [$link_tor_host queue]
    $queue_tor_host set data_limit_ [expr $dataBufferFromTorToHost/2]
    $queue_tor_host set thresh_ $cK
    $queue_tor_host set maxthresh_ $cK
    $queue_tor_host set limit_ $cB
    $queue_tor_host set token_refresh_rate_ [expr $cCreditRate]
    
    # guarantee 33% 33% 33%
    $queue_tor_host set-quantum 0 $queue_quantum_default
    $queue_tor_host set-quantum 1 [expr $queue_quantum_default*2*$xpass_queue_weight]
    $queue_tor_host set-quantum 2 [expr $queue_quantum_default*2*(1-$xpass_queue_weight)]
    $queue_tor_host set-thresh 1 $K_xpass
    $queue_tor_host set-thresh 2 $K_port
    $queue_tor_host egress-node $bcmTor($torIndex)

    if {$enable_queue_trace} {
      $queue_tor_host set trace_ 1
      $queue_tor_host set qidx_ $traceQueueCnt
      $queue_tor_host set exp_id_ $expID
      set ql_out [open "outputs/queue_exp${expID}_$traceQueueCnt.tr" w]
      puts $ql_out "Now, Qavg, Qmax"
      close $ql_out
      set traceQueueCnt [expr $traceQueueCnt + 1]
    }
  } else {
    $ns simplex-link $dcNode($i) $dcTor($torIndex) [set cLinkRate]Gb [expr $linkDelayHostTor+$hostDelay] DropTail
    set link_host_tor [$ns link $dcNode($i) $dcTor($torIndex)]
    set queue_host_tor [$link_host_tor queue]
    $queue_host_tor set limit_ $cB
    $ns simplex-link $dcTor($torIndex) $dcNode($i) [set cLinkRate]Gb $linkDelayHostTor XPassRED
    set link_tor_host [$ns link $dcTor($torIndex) $dcNode($i)]
    set queue_tor_host [$link_tor_host queue]
    $queue_tor_host set data_limit_ $dataBufferFromTorToHost
    $queue_tor_host set thresh_ $cK
    $queue_tor_host set maxthresh_ $cK
    $queue_tor_host set limit_ $cB
    $queue_tor_host set token_refresh_rate_ $cCreditRate

    if {$enable_queue_trace} {
      $queue_tor_host set trace_ 1
      $queue_tor_host set qidx_ $traceQueueCnt
      $queue_tor_host set exp_id_ $expID
      set ql_out [open "outputs/queue_exp${expID}_$traceQueueCnt.tr" w]
      puts $ql_out "Now, Qavg, Qmax"
      close $ql_out
      set traceQueueCnt [expr $traceQueueCnt + 1]
    }
  }
}

puts "Creating agents and flows..."

Agent/TCP set ecn_ 1
Agent/TCP set old_ecn_ 1
Agent/TCP set packetSize_ 1454
Agent/TCP set window_ 180
Agent/TCP set slow_start_restart_ false
Agent/TCP set tcpTick_ 0.000001
Agent/TCP set minrto_ 0.004
Agent/TCP set rtxcur_init_ 0.001
Agent/TCP set windowOption_ 0
Agent/TCP set tcpip_base_hdr_size_ 84

Agent/TCP set dctcp_ true
Agent/TCP set dctcp_g_ 0.0625

Agent/TCP set windowInit_ $tcpInitWindow ; # 10 MSS

Agent/TCP/FullTcp set segsize_ 1454
Agent/TCP/FullTcp set segsperack_ 1
Agent/TCP/FullTcp set spa_thresh_ 3000
Agent/TCP/FullTcp set interval_ 0
Agent/TCP/FullTcp set nodelay_ true
Agent/TCP/FullTcp set state_ 0
Agent/TCP/FullTcp set exp_id_ $expID


Agent/XPass set min_credit_size_ $minCreditSize
Agent/XPass set max_credit_size_ $maxCreditSize
Agent/XPass set min_ethernet_size_ $minEthernetSize
Agent/XPass set max_ethernet_size_ $maxEthernetSize
Agent/XPass set max_credit_rate_ $creditRate
Agent/XPass set base_credit_rate_ $baseCreditRate
Agent/XPass set target_loss_scaling_ 0.125
Agent/XPass set alpha_ $alpha
Agent/XPass set w_init_ $w_init 
Agent/XPass set min_w_ 0.01
Agent/XPass set retransmit_timeout_ 0.0001
Agent/XPass set min_jitter_ $minJitter
Agent/XPass set max_jitter_ $maxJitter
Agent/XPass set exp_id_ $expID
Agent/XPass set default_credit_stop_timeout_ 0.001 ;# 1ms
Agent/XPass set xpass_hdr_size_ $xpassHdrSize

Agent/TCP/FullTcp/XPass set min_credit_size_ $minCreditSize
Agent/TCP/FullTcp/XPass set max_credit_size_ $maxCreditSize
Agent/TCP/FullTcp/XPass set min_ethernet_size_ $minEthernetSize
Agent/TCP/FullTcp/XPass set max_ethernet_size_ $maxEthernetSize
Agent/TCP/FullTcp/XPass set max_credit_rate_ $creditRate
Agent/TCP/FullTcp/XPass set base_credit_rate_ $baseCreditRate
Agent/TCP/FullTcp/XPass set target_loss_scaling_ 0.125
Agent/TCP/FullTcp/XPass set alpha_ $alpha
Agent/TCP/FullTcp/XPass set w_init_ $w_init 
Agent/TCP/FullTcp/XPass set min_w_ 0.01
Agent/TCP/FullTcp/XPass set retransmit_timeout_ 0.0001
Agent/TCP/FullTcp/XPass set min_jitter_ $minJitter
Agent/TCP/FullTcp/XPass set max_jitter_ $maxJitter
Agent/TCP/FullTcp/XPass set exp_id_ $expID
Agent/TCP/FullTcp/XPass set default_credit_stop_timeout_ 0.001 ;# 1ms
Agent/TCP/FullTcp/XPass set xpass_hdr_size_ $xpassHdrSize

Agent/TCP/FullTcp/FlexPass set min_credit_size_ $minCreditSize
Agent/TCP/FullTcp/FlexPass set max_credit_size_ $maxCreditSize
Agent/TCP/FullTcp/FlexPass set min_ethernet_size_ $minEthernetSize
Agent/TCP/FullTcp/FlexPass set max_ethernet_size_ $maxEthernetSize
Agent/TCP/FullTcp/FlexPass set max_credit_rate_ $creditRate
Agent/TCP/FullTcp/FlexPass set base_credit_rate_ $baseCreditRate
Agent/TCP/FullTcp/FlexPass set target_loss_scaling_ 0.125
Agent/TCP/FullTcp/FlexPass set alpha_ $alpha
Agent/TCP/FullTcp/FlexPass set w_init_ $w_init 
Agent/TCP/FullTcp/FlexPass set min_w_ 0.01
Agent/TCP/FullTcp/FlexPass set retransmit_timeout_ 0.0001
Agent/TCP/FullTcp/FlexPass set min_jitter_ $minJitter
Agent/TCP/FullTcp/FlexPass set max_jitter_ $maxJitter
Agent/TCP/FullTcp/FlexPass set exp_id_ $expID
Agent/TCP/FullTcp/FlexPass set default_credit_stop_timeout_ 0.001 ;# 1ms
Agent/TCP/FullTcp/FlexPass set xpass_hdr_size_ $xpassHdrSize
Agent/TCP/FullTcp/FlexPass set flexpass_beta_ $actual_xpass_queue_weight
Agent/TCP/FullTcp/FlexPass set new_allocation_logic_ $newFlexPassAllocationLogic;
Agent/TCP/FullTcp/FlexPass set reordering_measure_in_rc3_ $reordering_measure_in_rc3_;
Agent/TCP/FullTcp/FlexPass set static_allocation_ $static_allocation_;
Agent/TCP/FullTcp/FlexPass set windowInit_ $flexpassReactiveInitWindow; #initial window

set numFlowForegroundEstiamte [expr ($numFlow - ($numFlow % ($numNode-1)))]
set numForegroundBundle [expr ($numNode-1) * $numFlowPerHost]

for {set i 0} {$i < [expr $numFlow + $numFlowForegroundEstiamte]} {incr i} {
  if {$i < $numFlow} {
    # Background
    set src_nodeid [expr int([$randomSrcNodeId value])]
    set dst_nodeid [expr int([$randomDstNodeId value])]

    while {$src_nodeid == $dst_nodeid} {
  #    set src_nodeid [expr int([$randomSrcNodeId value])]
      set dst_nodeid [expr int([$randomDstNodeId value])]
    }
  } else {
    # Foreground
    if {[expr ($i - $numFlow) % $numForegroundBundle] == 0} {
      set src_nodeid [expr int([$randomSrcNodeId value])]
    }
    set tmpid [expr ($i - $numFlow) % ($numNode-1)]
    if {$tmpid >= $src_nodeid} {
      set dst_nodeid [expr $tmpid + 1];
    } else {
      set dst_nodeid $tmpid;
    }
  }

  set locality [$randomDeterLocal value];
  if {$locality < $trafficLocality && $i < $numFlow} {
    set dst_nodeoff [expr int([$randomLocalOffset value])]
    set dst_nodeid [expr $src_nodeid-($src_nodeid%$numNodeInCluster)+$dst_nodeoff]
    while {$src_nodeid == $dst_nodeid} {
      set dst_nodeoff [expr int([$randomLocalOffset value])]
      set dst_nodeid [expr $src_nodeid-($src_nodeid%$numNodeInCluster)+$dst_nodeoff]
    }
#    puts "Intra-cluster Traffic, src=$src_nodeid, dst=$dst_nodeid" 
  }

  set src_cluster [expr $src_nodeid/($numNode/$numTor)/4]
  set dst_cluster [expr $dst_nodeid/($numNode/$numTor)/4]

  
  if {$deploy_per_cluster} {
    set standard [expr $src_cluster < [expr 2*$deployStep] && $dst_cluster < [expr 2*$deployStep]]  
  } else {
    set standard [expr [$randomDeployment value] <= [expr $deployStep/4.0]]
  }

  if {$standard} {
#   puts "cluster : xpass-xpass" 
    if {[string compare $ccc_type "xpass"] == 0} {
        # puts "xpass"
        set sender($i) [new Agent/XPass]
        set receiver($i) [new Agent/XPass]
    } elseif {[string compare $ccc_type "gdx"] == 0} {
        # puts "gdx"
        set sender($i) [new Agent/TCP/FullTcp/XPass]
        set receiver($i) [new Agent/TCP/FullTcp/XPass]
    } elseif {[string compare $ccc_type "flexpass"] == 0} {
        # puts "flexpass"
        set sender($i) [new Agent/TCP/FullTcp/FlexPass]
        set receiver($i) [new Agent/TCP/FullTcp/FlexPass]
    }
    set gdx_sid $i
  } else {
    
    # puts "tcp"
#   puts "cluster : dctcp-dctcp"
    set sender($i) [new Agent/TCP/FullTcp]
    set receiver($i) [new Agent/TCP/FullTcp]
  }
  
  # if {$src_cluster == 0 || $src_cluster == 2} {
  #   $sender($i) set max_credit_rate_ $creditRateHigh
  #   $sender($i) set dctcp_g_ 0.03125
  # } else {
    $sender($i) set max_credit_rate_ $creditRate
    $sender($i) set dctcp_g_ 0.0625
  # }

  # if {$dst_cluster == 0 || $dst_cluster == 2} {
  #   $receiver($i) set max_credit_rate_ $creditRateHigh
  #   $receiver($i) set dctcp_g_ 0.03125
  # } else {
    $receiver($i) set max_credit_rate_ $creditRate
    $receiver($i) set dctcp_g_ 0.0625
  # }

  $sender($i) set fid_ $i
  $sender($i) set host_id_ $src_nodeid
  $receiver($i) set fid_ $i
  $receiver($i) set host_id_ $dst_nodeid
 
  $receiver($i) listen

  $ns attach-agent $dcNode($src_nodeid) $sender($i)
  $ns attach-agent $dcNode($dst_nodeid) $receiver($i)
 
  $ns connect $sender($i) $receiver($i)

  $ns at $simEndTime "$sender($i) close"
  $ns at $simEndTime "$receiver($i) close"

  set srcIndex($i) $src_nodeid
  set dstIndex($i) $dst_nodeid
}

set nextTime $simStartTime
set fidx 0

set flowFinish 0
proc sendBytes {} {
  global ns random_flow_size nextTime sender fidx randomFlowSize randomFlowInterval numFlow srcIndex dstIndex flowfile
  while {1} {
    set fsize [expr ceil([expr [$randomFlowSize value]])]
    if {$fsize > 0} {
      break;
    }
  }

  puts $flowfile "$fidx $nextTime $srcIndex($fidx) $dstIndex($fidx) $fsize"
# puts "$nextTime $srcIndex($fidx) $dstIndex($fidx) $fsize"
  
  $ns at $nextTime "$sender($fidx) advance-bytes $fsize"
  
  $sender($fidx) set debug_flowisze_ $fsize
  # $receiver($fidx) set debug_flowisze_ $fsize

  set nextTime [expr $nextTime+[$randomFlowInterval value]]
  set fidx [expr $fidx+1]

  if {$fidx < $numFlow} {
    $ns at $nextTime "sendBytes"
  } else {
    set flowFinish 1;
  }
}

set nextTimeFG $simStartTime
set fidxFG $numFlow
proc sendForground {} {
  global ns random_flow_size nextTimeFG sender fidxFG randomFlowSize randomFlowInterval numFlow srcIndex dstIndex flowfile flowFinish

  if {$flowFinish != 0} {
    if {[expr $fidxFG % $numForegroundBundle] == 0} {
      set nextTimeFG [expr $nextTimeFG+$foregroundFlowInterval]
    }
  
    puts $flowfile "$fidxFG $nextTimeFG $srcIndex($fidxFG) $dstIndex($fidxFG) $foreground_flow_size"
    $ns at $nextTime "$sender($fidxFG) advance-bytes $foreground_flow_size"

    set fidxFG [expr $fidxFG+1]
    
    if {$fidxFg >= [expr $numFlow + $numFlowForegroundEstiamte]} {
      puts "numFlowForegroundEstiamte is too small! Try increasing"
      exit 1
    } else {
      $ns at $nextTimeFG "sendForground"
    }
  }
}

$ns at 0.0 "puts \"Simulation starts!\""
$ns at $nextTime "sendBytes"
if {$foreground_flow_ratio > 0} {
  $ns at $simStartTime "sendForground"
}
# if {$gdx_sid >= 0} {
#   $ns at [expr $simEndTime] "$receiver($gdx_sid) print-stat"
# }
$ns at [expr $simEndTime+0.5] "$bcmTor(0) print-egress-stat"
$ns at [expr $simEndTime+1] "finish"
$ns run
