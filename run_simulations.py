#!/usr/bin/env python

EXECUTABLE = "python ./ns2run.py"
EXTRA_ARGS = ""

cmdlines = [
    # search workload, background flows only
    f"{EXECUTABLE} -I 1000 {EXTRA_ARGS} --linkLoad 0.5 --foregroundFlowRatio 0.00 -c xpass --xpassQueueIsolation 0 --strictHighPriority 0 --workload search --deployStep 0", 
    f"{EXECUTABLE} -I 1001 {EXTRA_ARGS} --linkLoad 0.5 --foregroundFlowRatio 0.00 -c xpass --xpassQueueIsolation 0 --strictHighPriority 0 --workload search --deployStep 25", 
    f"{EXECUTABLE} -I 1002 {EXTRA_ARGS} --linkLoad 0.5 --foregroundFlowRatio 0.00 -c xpass --xpassQueueIsolation 0 --strictHighPriority 0 --workload search --deployStep 50", 
    f"{EXECUTABLE} -I 1003 {EXTRA_ARGS} --linkLoad 0.5 --foregroundFlowRatio 0.00 -c xpass --xpassQueueIsolation 0 --strictHighPriority 0 --workload search --deployStep 75", 
    f"{EXECUTABLE} -I 1004 {EXTRA_ARGS} --linkLoad 0.5 --foregroundFlowRatio 0.00 -c xpass --xpassQueueIsolation 0 --strictHighPriority 0 --workload search --deployStep 100", 
    f"{EXECUTABLE} -I 1010 {EXTRA_ARGS} --linkLoad 0.5 --foregroundFlowRatio 0.00 -c xpass --xpassQueueIsolation 1 --oracleQueueWeight 1 --strictHighPriority 0 --workload search --deployStep 0", 
    f"{EXECUTABLE} -I 1011 {EXTRA_ARGS} --linkLoad 0.5 --foregroundFlowRatio 0.00 -c xpass --xpassQueueIsolation 1 --oracleQueueWeight 1 --strictHighPriority 0 --workload search --deployStep 25", 
    f"{EXECUTABLE} -I 1012 {EXTRA_ARGS} --linkLoad 0.5 --foregroundFlowRatio 0.00 -c xpass --xpassQueueIsolation 1 --oracleQueueWeight 1 --strictHighPriority 0 --workload search --deployStep 50", 
    f"{EXECUTABLE} -I 1013 {EXTRA_ARGS} --linkLoad 0.5 --foregroundFlowRatio 0.00 -c xpass --xpassQueueIsolation 1 --oracleQueueWeight 1 --strictHighPriority 0 --workload search --deployStep 75", 
    f"{EXECUTABLE} -I 1014 {EXTRA_ARGS} --linkLoad 0.5 --foregroundFlowRatio 0.00 -c xpass --xpassQueueIsolation 1 --oracleQueueWeight 1 --strictHighPriority 0 --workload search --deployStep 100", 
    f"{EXECUTABLE} -I 1020 {EXTRA_ARGS} --linkLoad 0.5 --foregroundFlowRatio 0.00 -c gdx --workload search --deployStep 0", 
    f"{EXECUTABLE} -I 1021 {EXTRA_ARGS} --linkLoad 0.5 --foregroundFlowRatio 0.00 -c gdx --workload search --deployStep 25", 
    f"{EXECUTABLE} -I 1022 {EXTRA_ARGS} --linkLoad 0.5 --foregroundFlowRatio 0.00 -c gdx --workload search --deployStep 50", 
    f"{EXECUTABLE} -I 1023 {EXTRA_ARGS} --linkLoad 0.5 --foregroundFlowRatio 0.00 -c gdx --workload search --deployStep 75", 
    f"{EXECUTABLE} -I 1024 {EXTRA_ARGS} --linkLoad 0.5 --foregroundFlowRatio 0.00 -c gdx --workload search --deployStep 100", 
    f"{EXECUTABLE} -I 1030 {EXTRA_ARGS} --linkLoad 0.5 --foregroundFlowRatio 0.00 -c negdx --egdxScheme 2 --workload search --deployStep 0", 
    f"{EXECUTABLE} -I 1031 {EXTRA_ARGS} --linkLoad 0.5 --foregroundFlowRatio 0.00 -c negdx --egdxScheme 2 --workload search --deployStep 25", 
    f"{EXECUTABLE} -I 1032 {EXTRA_ARGS} --linkLoad 0.5 --foregroundFlowRatio 0.00 -c negdx --egdxScheme 2 --workload search --deployStep 50", 
    f"{EXECUTABLE} -I 1033 {EXTRA_ARGS} --linkLoad 0.5 --foregroundFlowRatio 0.00 -c negdx --egdxScheme 2 --workload search --deployStep 75", 
    f"{EXECUTABLE} -I 1034 {EXTRA_ARGS} --linkLoad 0.5 --foregroundFlowRatio 0.00 -c negdx --egdxScheme 2 --workload search --deployStep 100", 

    # search workload, mixed traffic (background + foreground)
    f"{EXECUTABLE} -I 2000 {EXTRA_ARGS} --linkLoad 0.5 --foregroundFlowRatio 0.10 -c xpass --xpassQueueIsolation 0 --strictHighPriority 0 --workload search --deployStep 0", 
    f"{EXECUTABLE} -I 2001 {EXTRA_ARGS} --linkLoad 0.5 --foregroundFlowRatio 0.10 -c xpass --xpassQueueIsolation 0 --strictHighPriority 0 --workload search --deployStep 25", 
    f"{EXECUTABLE} -I 2002 {EXTRA_ARGS} --linkLoad 0.5 --foregroundFlowRatio 0.10 -c xpass --xpassQueueIsolation 0 --strictHighPriority 0 --workload search --deployStep 50", 
    f"{EXECUTABLE} -I 2003 {EXTRA_ARGS} --linkLoad 0.5 --foregroundFlowRatio 0.10 -c xpass --xpassQueueIsolation 0 --strictHighPriority 0 --workload search --deployStep 75", 
    f"{EXECUTABLE} -I 2004 {EXTRA_ARGS} --linkLoad 0.5 --foregroundFlowRatio 0.10 -c xpass --xpassQueueIsolation 0 --strictHighPriority 0 --workload search --deployStep 100", 
    f"{EXECUTABLE} -I 2010 {EXTRA_ARGS} --linkLoad 0.5 --foregroundFlowRatio 0.10 -c xpass --xpassQueueIsolation 1 --oracleQueueWeight 1 --strictHighPriority 0 --workload search --deployStep 0", 
    f"{EXECUTABLE} -I 2011 {EXTRA_ARGS} --linkLoad 0.5 --foregroundFlowRatio 0.10 -c xpass --xpassQueueIsolation 1 --oracleQueueWeight 1 --strictHighPriority 0 --workload search --deployStep 25", 
    f"{EXECUTABLE} -I 2012 {EXTRA_ARGS} --linkLoad 0.5 --foregroundFlowRatio 0.10 -c xpass --xpassQueueIsolation 1 --oracleQueueWeight 1 --strictHighPriority 0 --workload search --deployStep 50", 
    f"{EXECUTABLE} -I 2013 {EXTRA_ARGS} --linkLoad 0.5 --foregroundFlowRatio 0.10 -c xpass --xpassQueueIsolation 1 --oracleQueueWeight 1 --strictHighPriority 0 --workload search --deployStep 75", 
    f"{EXECUTABLE} -I 2014 {EXTRA_ARGS} --linkLoad 0.5 --foregroundFlowRatio 0.10 -c xpass --xpassQueueIsolation 1 --oracleQueueWeight 1 --strictHighPriority 0 --workload search --deployStep 100", 
    f"{EXECUTABLE} -I 2020 {EXTRA_ARGS} --linkLoad 0.5 --foregroundFlowRatio 0.10 -c gdx --workload search --deployStep 0", 
    f"{EXECUTABLE} -I 2021 {EXTRA_ARGS} --linkLoad 0.5 --foregroundFlowRatio 0.10 -c gdx --workload search --deployStep 25", 
    f"{EXECUTABLE} -I 2022 {EXTRA_ARGS} --linkLoad 0.5 --foregroundFlowRatio 0.10 -c gdx --workload search --deployStep 50", 
    f"{EXECUTABLE} -I 2023 {EXTRA_ARGS} --linkLoad 0.5 --foregroundFlowRatio 0.10 -c gdx --workload search --deployStep 75", 
    f"{EXECUTABLE} -I 2024 {EXTRA_ARGS} --linkLoad 0.5 --foregroundFlowRatio 0.10 -c gdx --workload search --deployStep 100", 
    f"{EXECUTABLE} -I 2030 {EXTRA_ARGS} --linkLoad 0.5 --foregroundFlowRatio 0.10 -c negdx --egdxScheme 2 --workload search --deployStep 0", 
    f"{EXECUTABLE} -I 2031 {EXTRA_ARGS} --linkLoad 0.5 --foregroundFlowRatio 0.10 -c negdx --egdxScheme 2 --workload search --deployStep 25", 
    f"{EXECUTABLE} -I 2032 {EXTRA_ARGS} --linkLoad 0.5 --foregroundFlowRatio 0.10 -c negdx --egdxScheme 2 --workload search --deployStep 50", 
    f"{EXECUTABLE} -I 2033 {EXTRA_ARGS} --linkLoad 0.5 --foregroundFlowRatio 0.10 -c negdx --egdxScheme 2 --workload search --deployStep 75", 
    f"{EXECUTABLE} -I 2034 {EXTRA_ARGS} --linkLoad 0.5 --foregroundFlowRatio 0.10 -c negdx --egdxScheme 2 --workload search --deployStep 100", 
    
    # link load scale (naive ExpressPass)
    f"{EXECUTABLE} -I 3010 {EXTRA_ARGS} --linkLoad 0.1 --foregroundFlowRatio 0.00 -c xpass --xpassQueueIsolation 0 --strictHighPriority 0 --workload search --deployStep 0", 
    f"{EXECUTABLE} -I 3011 {EXTRA_ARGS} --linkLoad 0.1 --foregroundFlowRatio 0.00 -c xpass --xpassQueueIsolation 0 --strictHighPriority 0 --workload search --deployStep 25", 
    f"{EXECUTABLE} -I 3012 {EXTRA_ARGS} --linkLoad 0.1 --foregroundFlowRatio 0.00 -c xpass --xpassQueueIsolation 0 --strictHighPriority 0 --workload search --deployStep 50", 
    f"{EXECUTABLE} -I 3013 {EXTRA_ARGS} --linkLoad 0.1 --foregroundFlowRatio 0.00 -c xpass --xpassQueueIsolation 0 --strictHighPriority 0 --workload search --deployStep 75", 
    f"{EXECUTABLE} -I 3014 {EXTRA_ARGS} --linkLoad 0.1 --foregroundFlowRatio 0.00 -c xpass --xpassQueueIsolation 0 --strictHighPriority 0 --workload search --deployStep 100", 
    f"{EXECUTABLE} -I 3020 {EXTRA_ARGS} --linkLoad 0.2 --foregroundFlowRatio 0.00 -c xpass --xpassQueueIsolation 0 --strictHighPriority 0 --workload search --deployStep 0", 
    f"{EXECUTABLE} -I 3021 {EXTRA_ARGS} --linkLoad 0.2 --foregroundFlowRatio 0.00 -c xpass --xpassQueueIsolation 0 --strictHighPriority 0 --workload search --deployStep 25", 
    f"{EXECUTABLE} -I 3022 {EXTRA_ARGS} --linkLoad 0.2 --foregroundFlowRatio 0.00 -c xpass --xpassQueueIsolation 0 --strictHighPriority 0 --workload search --deployStep 50", 
    f"{EXECUTABLE} -I 3023 {EXTRA_ARGS} --linkLoad 0.2 --foregroundFlowRatio 0.00 -c xpass --xpassQueueIsolation 0 --strictHighPriority 0 --workload search --deployStep 75", 
    f"{EXECUTABLE} -I 3024 {EXTRA_ARGS} --linkLoad 0.2 --foregroundFlowRatio 0.00 -c xpass --xpassQueueIsolation 0 --strictHighPriority 0 --workload search --deployStep 100", 
    f"{EXECUTABLE} -I 3030 {EXTRA_ARGS} --linkLoad 0.3 --foregroundFlowRatio 0.00 -c xpass --xpassQueueIsolation 0 --strictHighPriority 0 --workload search --deployStep 0", 
    f"{EXECUTABLE} -I 3031 {EXTRA_ARGS} --linkLoad 0.3 --foregroundFlowRatio 0.00 -c xpass --xpassQueueIsolation 0 --strictHighPriority 0 --workload search --deployStep 25", 
    f"{EXECUTABLE} -I 3032 {EXTRA_ARGS} --linkLoad 0.3 --foregroundFlowRatio 0.00 -c xpass --xpassQueueIsolation 0 --strictHighPriority 0 --workload search --deployStep 50", 
    f"{EXECUTABLE} -I 3033 {EXTRA_ARGS} --linkLoad 0.3 --foregroundFlowRatio 0.00 -c xpass --xpassQueueIsolation 0 --strictHighPriority 0 --workload search --deployStep 75", 
    f"{EXECUTABLE} -I 3034 {EXTRA_ARGS} --linkLoad 0.3 --foregroundFlowRatio 0.00 -c xpass --xpassQueueIsolation 0 --strictHighPriority 0 --workload search --deployStep 100", 
    f"{EXECUTABLE} -I 3040 {EXTRA_ARGS} --linkLoad 0.4 --foregroundFlowRatio 0.00 -c xpass --xpassQueueIsolation 0 --strictHighPriority 0 --workload search --deployStep 0", 
    f"{EXECUTABLE} -I 3041 {EXTRA_ARGS} --linkLoad 0.4 --foregroundFlowRatio 0.00 -c xpass --xpassQueueIsolation 0 --strictHighPriority 0 --workload search --deployStep 25", 
    f"{EXECUTABLE} -I 3042 {EXTRA_ARGS} --linkLoad 0.4 --foregroundFlowRatio 0.00 -c xpass --xpassQueueIsolation 0 --strictHighPriority 0 --workload search --deployStep 50", 
    f"{EXECUTABLE} -I 3043 {EXTRA_ARGS} --linkLoad 0.4 --foregroundFlowRatio 0.00 -c xpass --xpassQueueIsolation 0 --strictHighPriority 0 --workload search --deployStep 75", 
    f"{EXECUTABLE} -I 3044 {EXTRA_ARGS} --linkLoad 0.4 --foregroundFlowRatio 0.00 -c xpass --xpassQueueIsolation 0 --strictHighPriority 0 --workload search --deployStep 100", 
    f"{EXECUTABLE} -I 3060 {EXTRA_ARGS} --linkLoad 0.6 --foregroundFlowRatio 0.00 -c xpass --xpassQueueIsolation 0 --strictHighPriority 0 --workload search --deployStep 0", 
    f"{EXECUTABLE} -I 3061 {EXTRA_ARGS} --linkLoad 0.6 --foregroundFlowRatio 0.00 -c xpass --xpassQueueIsolation 0 --strictHighPriority 0 --workload search --deployStep 25", 
    f"{EXECUTABLE} -I 3062 {EXTRA_ARGS} --linkLoad 0.6 --foregroundFlowRatio 0.00 -c xpass --xpassQueueIsolation 0 --strictHighPriority 0 --workload search --deployStep 50", 
    f"{EXECUTABLE} -I 3063 {EXTRA_ARGS} --linkLoad 0.6 --foregroundFlowRatio 0.00 -c xpass --xpassQueueIsolation 0 --strictHighPriority 0 --workload search --deployStep 75", 
    f"{EXECUTABLE} -I 3064 {EXTRA_ARGS} --linkLoad 0.6 --foregroundFlowRatio 0.00 -c xpass --xpassQueueIsolation 0 --strictHighPriority 0 --workload search --deployStep 100", 
    f"{EXECUTABLE} -I 3070 {EXTRA_ARGS} --linkLoad 0.7 --foregroundFlowRatio 0.00 -c xpass --xpassQueueIsolation 0 --strictHighPriority 0 --workload search --deployStep 0", 
    f"{EXECUTABLE} -I 3071 {EXTRA_ARGS} --linkLoad 0.7 --foregroundFlowRatio 0.00 -c xpass --xpassQueueIsolation 0 --strictHighPriority 0 --workload search --deployStep 25", 
    f"{EXECUTABLE} -I 3072 {EXTRA_ARGS} --linkLoad 0.7 --foregroundFlowRatio 0.00 -c xpass --xpassQueueIsolation 0 --strictHighPriority 0 --workload search --deployStep 50", 
    f"{EXECUTABLE} -I 3073 {EXTRA_ARGS} --linkLoad 0.7 --foregroundFlowRatio 0.00 -c xpass --xpassQueueIsolation 0 --strictHighPriority 0 --workload search --deployStep 75", 
    f"{EXECUTABLE} -I 3074 {EXTRA_ARGS} --linkLoad 0.7 --foregroundFlowRatio 0.00 -c xpass --xpassQueueIsolation 0 --strictHighPriority 0 --workload search --deployStep 100", 
    
    # link load scale (FlexPass)
    f"{EXECUTABLE} -I 4010 {EXTRA_ARGS} --linkLoad 0.1 --foregroundFlowRatio 0.00 -c negdx --egdxScheme 2 --workload search --deployStep 0", 
    f"{EXECUTABLE} -I 4011 {EXTRA_ARGS} --linkLoad 0.1 --foregroundFlowRatio 0.00 -c negdx --egdxScheme 2 --workload search --deployStep 25", 
    f"{EXECUTABLE} -I 4012 {EXTRA_ARGS} --linkLoad 0.1 --foregroundFlowRatio 0.00 -c negdx --egdxScheme 2 --workload search --deployStep 50", 
    f"{EXECUTABLE} -I 4013 {EXTRA_ARGS} --linkLoad 0.1 --foregroundFlowRatio 0.00 -c negdx --egdxScheme 2 --workload search --deployStep 75", 
    f"{EXECUTABLE} -I 4014 {EXTRA_ARGS} --linkLoad 0.1 --foregroundFlowRatio 0.00 -c negdx --egdxScheme 2 --workload search --deployStep 100", 
    f"{EXECUTABLE} -I 4020 {EXTRA_ARGS} --linkLoad 0.2 --foregroundFlowRatio 0.00 -c negdx --egdxScheme 2 --workload search --deployStep 0", 
    f"{EXECUTABLE} -I 4021 {EXTRA_ARGS} --linkLoad 0.2 --foregroundFlowRatio 0.00 -c negdx --egdxScheme 2 --workload search --deployStep 25", 
    f"{EXECUTABLE} -I 4022 {EXTRA_ARGS} --linkLoad 0.2 --foregroundFlowRatio 0.00 -c negdx --egdxScheme 2 --workload search --deployStep 50", 
    f"{EXECUTABLE} -I 4023 {EXTRA_ARGS} --linkLoad 0.2 --foregroundFlowRatio 0.00 -c negdx --egdxScheme 2 --workload search --deployStep 75", 
    f"{EXECUTABLE} -I 4024 {EXTRA_ARGS} --linkLoad 0.2 --foregroundFlowRatio 0.00 -c negdx --egdxScheme 2 --workload search --deployStep 100", 
    f"{EXECUTABLE} -I 4030 {EXTRA_ARGS} --linkLoad 0.3 --foregroundFlowRatio 0.00 -c negdx --egdxScheme 2 --workload search --deployStep 0", 
    f"{EXECUTABLE} -I 4031 {EXTRA_ARGS} --linkLoad 0.3 --foregroundFlowRatio 0.00 -c negdx --egdxScheme 2 --workload search --deployStep 25", 
    f"{EXECUTABLE} -I 4032 {EXTRA_ARGS} --linkLoad 0.3 --foregroundFlowRatio 0.00 -c negdx --egdxScheme 2 --workload search --deployStep 50", 
    f"{EXECUTABLE} -I 4033 {EXTRA_ARGS} --linkLoad 0.3 --foregroundFlowRatio 0.00 -c negdx --egdxScheme 2 --workload search --deployStep 75", 
    f"{EXECUTABLE} -I 4034 {EXTRA_ARGS} --linkLoad 0.3 --foregroundFlowRatio 0.00 -c negdx --egdxScheme 2 --workload search --deployStep 100", 
    f"{EXECUTABLE} -I 4040 {EXTRA_ARGS} --linkLoad 0.4 --foregroundFlowRatio 0.00 -c negdx --egdxScheme 2 --workload search --deployStep 0", 
    f"{EXECUTABLE} -I 4041 {EXTRA_ARGS} --linkLoad 0.4 --foregroundFlowRatio 0.00 -c negdx --egdxScheme 2 --workload search --deployStep 25", 
    f"{EXECUTABLE} -I 4042 {EXTRA_ARGS} --linkLoad 0.4 --foregroundFlowRatio 0.00 -c negdx --egdxScheme 2 --workload search --deployStep 50", 
    f"{EXECUTABLE} -I 4043 {EXTRA_ARGS} --linkLoad 0.4 --foregroundFlowRatio 0.00 -c negdx --egdxScheme 2 --workload search --deployStep 75", 
    f"{EXECUTABLE} -I 4044 {EXTRA_ARGS} --linkLoad 0.4 --foregroundFlowRatio 0.00 -c negdx --egdxScheme 2 --workload search --deployStep 100", 
    f"{EXECUTABLE} -I 4060 {EXTRA_ARGS} --linkLoad 0.6 --foregroundFlowRatio 0.00 -c negdx --egdxScheme 2 --workload search --deployStep 0", 
    f"{EXECUTABLE} -I 4061 {EXTRA_ARGS} --linkLoad 0.6 --foregroundFlowRatio 0.00 -c negdx --egdxScheme 2 --workload search --deployStep 25", 
    f"{EXECUTABLE} -I 4062 {EXTRA_ARGS} --linkLoad 0.6 --foregroundFlowRatio 0.00 -c negdx --egdxScheme 2 --workload search --deployStep 50", 
    f"{EXECUTABLE} -I 4063 {EXTRA_ARGS} --linkLoad 0.6 --foregroundFlowRatio 0.00 -c negdx --egdxScheme 2 --workload search --deployStep 75", 
    f"{EXECUTABLE} -I 4064 {EXTRA_ARGS} --linkLoad 0.6 --foregroundFlowRatio 0.00 -c negdx --egdxScheme 2 --workload search --deployStep 100", 
    f"{EXECUTABLE} -I 4070 {EXTRA_ARGS} --linkLoad 0.7 --foregroundFlowRatio 0.00 -c negdx --egdxScheme 2 --workload search --deployStep 0", 
    f"{EXECUTABLE} -I 4071 {EXTRA_ARGS} --linkLoad 0.7 --foregroundFlowRatio 0.00 -c negdx --egdxScheme 2 --workload search --deployStep 25", 
    f"{EXECUTABLE} -I 4072 {EXTRA_ARGS} --linkLoad 0.7 --foregroundFlowRatio 0.00 -c negdx --egdxScheme 2 --workload search --deployStep 50", 
    f"{EXECUTABLE} -I 4073 {EXTRA_ARGS} --linkLoad 0.7 --foregroundFlowRatio 0.00 -c negdx --egdxScheme 2 --workload search --deployStep 75", 
    f"{EXECUTABLE} -I 4074 {EXTRA_ARGS} --linkLoad 0.7 --foregroundFlowRatio 0.00 -c negdx --egdxScheme 2 --workload search --deployStep 100", 
]


# Below is simple script to run multiple simulations concurrently

import os, asyncio, uuid
async def main():
    uid = uuid.uuid4()
    os.mkdir(f"/tmp/{uid}")
    async def run(idx, cmdline):    
        async with semaphore:
            proc = await asyncio.create_subprocess_shell(cmdline, stdout=open(f"/tmp/{uid}/{idx}.stdout", "w"), stderr=open(f"/tmp/{uid}/{idx}.stderr", "w"))
            print(f">> [Simulation #{idx} (PID {proc.pid})]", cmdline)
            await proc.wait()
            print(f">> [Simulation #{idx} (PID {proc.pid})] Finished.")
    semaphore = asyncio.Semaphore(max(len(os.sched_getaffinity(0)), 1))
    await asyncio.gather(*[asyncio.ensure_future(run(idx, c)) for idx, c in enumerate(cmdlines)])
if __name__ == '__main__':
    asyncio.run(main())