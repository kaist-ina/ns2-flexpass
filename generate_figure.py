#!/usr/bin/python3
import sys
import os
import numpy as np
import pandas as pd
from typing import Optional, Tuple, Union
from dataclasses import dataclass
from argparse import ArgumentParser

def main():
    parser = ArgumentParser()
    parser.add_argument("--output-dir", default="outputs")
    args = parser.parse_args()

    source_bgonly = pd.DataFrame({
        'naive':    ['outputs/fct_1000.out', 'outputs/fct_1001.out', 'outputs/fct_1002.out', 'outputs/fct_1003.out', 'outputs/fct_1004.out'],
        'oWF':      ['outputs/fct_1010.out', 'outputs/fct_1011.out', 'outputs/fct_1012.out', 'outputs/fct_1013.out', 'outputs/fct_1014.out'],
        'LY':       ['outputs/fct_1020.out', 'outputs/fct_1021.out', 'outputs/fct_1022.out', 'outputs/fct_1023.out', 'outputs/fct_1024.out'],
        'FlexPass': ['outputs/fct_1030.out', 'outputs/fct_1031.out', 'outputs/fct_1032.out', 'outputs/fct_1033.out', 'outputs/fct_1034.out'],
    }, index=[0, 0.25, 0.5, 0.75, 1])

    source_mixed = pd.DataFrame({
        'naive':    ['outputs/fct_2000.out', 'outputs/fct_2001.out', 'outputs/fct_2002.out', 'outputs/fct_2003.out', 'outputs/fct_2004.out'],
        'oWF':      ['outputs/fct_2010.out', 'outputs/fct_2011.out', 'outputs/fct_2012.out', 'outputs/fct_2013.out', 'outputs/fct_2014.out'],
        'LY':       ['outputs/fct_2020.out', 'outputs/fct_2021.out', 'outputs/fct_2022.out', 'outputs/fct_2023.out', 'outputs/fct_2024.out'],
        'FlexPass': ['outputs/fct_2030.out', 'outputs/fct_2031.out', 'outputs/fct_2032.out', 'outputs/fct_2033.out', 'outputs/fct_2034.out'],
    }, index=[0, 0.25, 0.5, 0.75, 1])
    
    source_bgonly_expresspass_load_scale = pd.DataFrame({
        'Load 10%':    ['outputs/fct_3010.out', 'outputs/fct_3011.out', 'outputs/fct_3012.out', 'outputs/fct_3013.out', 'outputs/fct_3014.out'],
        'Load 20%':    ['outputs/fct_3020.out', 'outputs/fct_3021.out', 'outputs/fct_3022.out', 'outputs/fct_3023.out', 'outputs/fct_3024.out'],
        'Load 30%':    ['outputs/fct_3030.out', 'outputs/fct_3031.out', 'outputs/fct_3032.out', 'outputs/fct_3033.out', 'outputs/fct_3034.out'],
        'Load 40%':    ['outputs/fct_3040.out', 'outputs/fct_3041.out', 'outputs/fct_3042.out', 'outputs/fct_3043.out', 'outputs/fct_3044.out'],
        'Load 50%':    ['outputs/fct_1000.out', 'outputs/fct_1001.out', 'outputs/fct_1002.out', 'outputs/fct_1003.out', 'outputs/fct_1004.out'],
        'Load 60%':    ['outputs/fct_3060.out', 'outputs/fct_3061.out', 'outputs/fct_3062.out', 'outputs/fct_3063.out', 'outputs/fct_3064.out'],
        'Load 70%':    ['outputs/fct_3070.out', 'outputs/fct_3071.out', 'outputs/fct_3072.out', 'outputs/fct_3073.out', 'outputs/fct_3074.out'],
    }, index=[0, 0.25, 0.5, 0.75, 1])

    source_bgonly_flexpass_load_scale = pd.DataFrame({
        'Load 10%':    ['outputs/fct_4010.out', 'outputs/fct_4011.out', 'outputs/fct_4012.out', 'outputs/fct_4013.out', 'outputs/fct_4014.out'],
        'Load 20%':    ['outputs/fct_4020.out', 'outputs/fct_4021.out', 'outputs/fct_4022.out', 'outputs/fct_4023.out', 'outputs/fct_4024.out'],
        'Load 30%':    ['outputs/fct_4030.out', 'outputs/fct_4031.out', 'outputs/fct_4032.out', 'outputs/fct_4033.out', 'outputs/fct_4034.out'],
        'Load 40%':    ['outputs/fct_4040.out', 'outputs/fct_4041.out', 'outputs/fct_4042.out', 'outputs/fct_4043.out', 'outputs/fct_4044.out'],
        'Load 50%':    ['outputs/fct_1030.out', 'outputs/fct_1031.out', 'outputs/fct_1032.out', 'outputs/fct_1033.out', 'outputs/fct_1034.out'],
        'Load 60%':    ['outputs/fct_4060.out', 'outputs/fct_4061.out', 'outputs/fct_4062.out', 'outputs/fct_4063.out', 'outputs/fct_4064.out'],
        'Load 70%':    ['outputs/fct_4070.out', 'outputs/fct_4071.out', 'outputs/fct_4072.out', 'outputs/fct_4073.out', 'outputs/fct_4074.out'],
    }, index=[0, 0.25, 0.5, 0.75, 1])

    color_theme = ('#F69200', '#A6B727', '#7030A0', '#0174F0')
    color1_gradation = ('#FFEACA', '#FFD495', '#FFBF61', '#F69200', '#B96D00', '#7B4900', '#000000')
    color2_gradation = ('#D8E8F1', '#B1D1E3', '#8ABAD4', '#418AB3', '#316886', '#20455A', '#000000')

    list_figure = [
        FigureDef(name="fig10a", metric="fct_small_99%",    source=source_bgonly, yrange=(0.5, 2.5), colors=color_theme),
        FigureDef(name="fig10b", metric="fct_overall_avg",  source=source_bgonly, yrange=(10, 16), colors=color_theme),

        FigureDef(name="fig11a", metric="fct_small_99%",    source=source_mixed, yrange=(0, 2), colors=color_theme),
        FigureDef(name="fig11b", metric="fct_overall_avg",  source=source_mixed, yrange=(8, 16), colors=color_theme),

        FigureDef(name="fig12a", metric={"DCTCP": "fct_small_99%_legacy", "ExpressPass": "fct_small_99%_new"},
                  source=source_bgonly[['naive']], yrange=(0, 3), colors=('#000000', color_theme[0])),
        FigureDef(name="fig12b", metric={"DCTCP": "fct_small_99%_legacy", "oWF": "fct_small_99%_new"},
                  source=source_bgonly[['oWF']], yrange=(0, 3), colors=('#000000', color_theme[1])),
        FigureDef(name="fig12c", metric={"DCTCP": "fct_small_99%_legacy", "LY": "fct_small_99%_new"},
                  source=source_bgonly[['LY']], yrange=(0, 3), colors=('#000000', color_theme[2])),
        FigureDef(name="fig12d", metric={"DCTCP": "fct_small_99%_legacy", "FlexPass": "fct_small_99%_new"},
                  source=source_bgonly[['FlexPass']], yrange=(0, 3), colors=('#000000', color_theme[3])),

        FigureDef(name="fig13a", metric={"DCTCP": "fct_small_std_legacy", "ExpressPass": "fct_small_std_new"},
                  source=source_bgonly[['naive']], yrange=(0, 0.6), colors=('#000000', color_theme[0])),
        FigureDef(name="fig13b", metric={"DCTCP": "fct_small_std_legacy", "oWF": "fct_small_std_new"},
                  source=source_bgonly[['oWF']], yrange=(0, 0.6), colors=('#000000', color_theme[1])),
        FigureDef(name="fig13c", metric={"DCTCP": "fct_small_std_legacy", "LY": "fct_small_std_new"},
                  source=source_bgonly[['LY']], yrange=(0, 0.6), colors=('#000000', color_theme[2])),
        FigureDef(name="fig13d", metric={"DCTCP": "fct_small_std_legacy", "FlexPass": "fct_small_std_new"},
                  source=source_bgonly[['FlexPass']], yrange=(0, 0.6), colors=('#000000', color_theme[3])),

        FigureDef(name="fig14a", metric="fct_small_99%",    source=source_bgonly_expresspass_load_scale,
                  yrange=(0, 5), colors=color1_gradation),
        FigureDef(name="fig14b", metric="fct_small_99%",    source=source_bgonly_flexpass_load_scale,
                  yrange=(0, 5), colors=color2_gradation),
    ]
    
    for figure in list_figure:
        save_figure(figure, args)


@dataclass
class FigureDef:
    name: str
    metric: Union[str, dict]
    source: pd.DataFrame
    yrange: Tuple[Optional[float], Optional[float]] = (None, None)
    colors: Optional[Tuple] = None


def get_datapoint(filename: str, metric: str) -> float:
    return int("".join([x for x in filename if x.isnumeric()]))


SMALL_FLOW_THRESH = 100000 # 100kB
def get_datapoint(filename: str, metric: str) -> float:
    fcts_small_flow = []
    fcts_overall = []
    flow_sizes = []
    fcts_small_flow_legacy = []
    fcts_small_flow_new = []

    with open(filename, "r") as f:
        fcnt = 0
        for line in f:
            elem = line.split(",")
            if len(elem) < 3: continue
            try:
                fsize = int(elem[1])
                fct = float(elem[2]) * 1000 # in ms
            except ValueError:
                continue

            flow_sizes.append(fsize)
            if fsize < SMALL_FLOW_THRESH:
                fcts_small_flow.append(fct)
                if 'tcp' in line:
                    fcts_small_flow_legacy.append(fct)
                elif 'gdx' in line or 'xpass' in line:
                    fcts_small_flow_new.append(fct)
            fcts_overall.append(fct)

            if fct > 0:
                fcnt += 1
    flow_sizes = np.array(flow_sizes)
    fcts_small_flow = np.array(fcts_small_flow)
    fcts_overall = np.array(fcts_overall)

    if metric == 'fct_small_99%':
        return np.percentile(fcts_small_flow, 99) if len(fcts_small_flow) > 0 else float('nan')
    if metric == 'fct_small_99.9%':
        return np.percentile(fcts_small_flow, 99.9) if len(fcts_small_flow) > 0 else float('nan')
    if metric == 'fct_overall_avg':
        return np.average(fcts_overall) if len(fcts_overall) > 0 else float('nan')
    if metric == 'fct_small_99%_legacy':
        return np.percentile(fcts_small_flow_legacy, 99) if len(fcts_small_flow_legacy) > 0 else float('nan')
    if metric == 'fct_small_99.9%_legacy':
        return np.percentile(fcts_small_flow_legacy, 99.9) if len(fcts_small_flow_legacy) > 0 else float('nan')
    if metric == 'fct_small_99%_new':
        return np.percentile(fcts_small_flow_new, 99) if len(fcts_small_flow_new) > 0 else float('nan')
    if metric == 'fct_small_99.9%_new':
        return np.percentile(fcts_small_flow_new, 99.9) if len(fcts_small_flow_new) > 0 else float('nan')
    if metric == 'fct_small_std_legacy':
        return np.std(fcts_small_flow_legacy) if len(fcts_small_flow_legacy) > 0 else float('nan')
    if metric == 'fct_small_std_new':
        return np.std(fcts_small_flow_new) if len(fcts_small_flow_new) > 0 else float('nan')
    raise NotImplementedError(f"Metric {metric} is not implemented.")

def get_yaxis_label(metric: str) -> str:
    if metric == 'fct_small_99%':
        return r'99%-ile FCT (ms)'
    if metric == 'fct_small_99.9%':
        return r'99.9%-ile FCT (ms)'
    if metric == 'fct_overall_avg':
        return r'Overall avg FCT (ms)'
    if metric == 'fct_small_99%_legacy':
        return r'99%-ile FCT (ms)'
    if metric == 'fct_small_99.9%_legacy':
        return r'99.9%-ile FCT (ms)'
    if metric == 'fct_small_99%_new':
        return r'99%-ile FCT (ms)'
    if metric == 'fct_small_99.9%_new':
        return r'99.9%-ile FCT (ms)'
    if metric == 'fct_small_std_legacy':
        return r'FCT Stdev. (ms)'
    if metric == 'fct_small_std_new':
        return r'FCT Stdev. (ms)'
    raise NotImplementedError(f"Metric {metric} is not implemented.")

def save_figure(figure: FigureDef, args):
    if type(figure.metric) == str:
        df = figure.source.applymap(lambda fn: get_datapoint(fn, figure.metric))
        ylabel = get_yaxis_label(figure.metric)
    elif type(figure.metric) == dict:
        df = {k: figure.source.applymap(lambda fn: get_datapoint(fn, metric)) for k, metric in figure.metric.items()}
        df = [v.rename(lambda n: f"{n}({k})", axis='columns') for k, v in df.items()]
        df = pd.concat(df, axis=1)
        ylabel = get_yaxis_label(list(figure.metric.values())[0])
    else:
        raise NotImplementedError("Metric must be str or dict.")

    print(df)
    df.to_csv(os.path.join(args.output_dir, f"{figure.name}.csv"))

    # draw figure
    plot = df.plot(style='.-', color=figure.colors, figsize=(5, 4))
    plot.set_ybound(lower=figure.yrange[0], upper=figure.yrange[1])
    plot.set_xlabel('Deployment %')
    plot.set_ylabel(ylabel)
    plot.xaxis.set_major_formatter(lambda x, _ : f"{int(x * 100)}%")
    plot.xaxis.set_ticks([0, 0.25, 0.5, 0.75, 1])
    plot.legend(bbox_to_anchor=(0, 1.02, 1, 0.2), loc="lower left",
                mode="expand", borderaxespad=0, ncol=4)
    fig = plot.get_figure()
    fig.savefig(os.path.join(args.output_dir, f"{figure.name}.png"))

if __name__ == "__main__":
    main()