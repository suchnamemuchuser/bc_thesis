#!/usr/local/bin/python3

import datetime as dt
import sys
import numpy as np

import matplotlib.dates as dates
import matplotlib.pyplot as plt

import astropy.units as u
from astropy.coordinates import AltAz,EarthLocation,SkyCoord
from astropy.time import Time

#fyzicke omezeni je min 1-357deg
#plna rychlost azimut ~ 29.5deg/min
AZ_MIN=29
AZ_MAX=355
#fyzicke omezeni je min 10-84deg
ALT_MIN=15
ALT_MAX=84

tgt_name='B0329+54'

#use command line param for name if there is any
if len(sys.argv) > 1: tgt_name = sys.argv[1]

try:
  tgt = SkyCoord.from_name('PSR '+tgt_name, parse=False)
except:
  print("warning, using only parsing for "+tgt_name)
  tgt = SkyCoord.from_name('PSR '+tgt_name, parse='True')

ondrejov = EarthLocation(lat=49.9085742*u.deg, lon=14.7797511*u.deg, height=512*u.m)
#get UTC time directly
utc_now = dt.datetime.utcnow()
#get local time for graphs
loc_now = dt.datetime.now()

#print("Evaluation start UTC: ", utc_now)
print("Target", tgt_name, "evaluation start loc: ", loc_now)

obs_start_utc = Time(utc_now)

#for position each 1min for one day
one_day = np.linspace(0, 24, 24*60+1)*u.hour

tgt_altaz = tgt.transform_to(AltAz(obstime=obs_start_utc+one_day,location=ondrejov))
#print(f"Target {tgt_name} initial altitude {tgt_altaz[0].alt:.2f}, azimuth {tgt_altaz[0].az: .2f}")

plt.figure(figsize=(10,5))
fig, (ax_az, ax_alt) = plt.subplots(2)

#dates for one day each 1min
x_dates = [loc_now+dt.timedelta(minutes=i) for i in range(0, 24*60+1)]

xfmt = dates.DateFormatter('%H:%M')
ax_az.xaxis.set_major_formatter(xfmt)
ax_alt.xaxis.set_major_formatter(xfmt)

major_ticks = np.arange(0, 361, 60)
minor_ticks = np.arange(0, 361, 20)

start_date=loc_now.strftime('%Y-%m-%d %H:%M')
ax_az.set_title(f'Observability of {tgt_name} starting {start_date}')

ax_az.grid(which='major', alpha=0.4)
ax_az.grid(which='minor', alpha=0.2)
ax_az.set_yticks(major_ticks)
ax_az.set_yticks(minor_ticks, minor=True)
ax_az.set_xticks(x_dates[::240])
ax_az.set_xticks(x_dates[::60], minor=True)

ax_az.set_ylim([0,360])
ax_az.scatter(x_dates, tgt_altaz.az, s=1, marker='.', label='Azimuth')
ax_az.fill_between(x_dates, AZ_MIN, AZ_MAX, 
       where=(np.logical_and(
        np.logical_and(tgt_altaz.az.value > AZ_MIN, tgt_altaz.az.value < AZ_MAX),
        np.logical_and(tgt_altaz.alt.value > ALT_MIN, tgt_altaz.alt.value < ALT_MAX)
        )) ,
                    alpha=0.1, zorder=0)

major_ticks = np.arange(0, 91, 20)
minor_ticks = np.arange(0, 91, 5)

ax_alt.grid(which='major', alpha=0.4)
ax_alt.grid(which='minor', alpha=0.2)
ax_alt.set_yticks(major_ticks)
ax_alt.set_yticks(minor_ticks, minor=True)
ax_alt.set_xticks(x_dates[::240])
ax_alt.set_xticks(x_dates[::60], minor=True)

ax_alt.set_ylim([0, 90])
ax_alt.scatter(x_dates, tgt_altaz.alt, s=1, marker='.', label='Altitude')
ax_alt.fill_between(x_dates, ALT_MIN, ALT_MAX,
       where=(np.logical_and(
        np.logical_and(tgt_altaz.az.value > AZ_MIN, tgt_altaz.az.value < AZ_MAX),
        np.logical_and(tgt_altaz.alt.value > ALT_MIN, tgt_altaz.alt.value < ALT_MAX)
        )) ,
                    alpha=0.1, zorder=0)

all_intervals = []

#get only acceptable time/azimuth/altitude
is_enabled=False;
for i in range(0, len(tgt_altaz)):
  if (tgt_altaz[i].az.value > AZ_MIN and tgt_altaz[i].az.value < AZ_MAX
    and tgt_altaz[i].alt.value > ALT_MIN and tgt_altaz[i].alt.value < ALT_MAX):
    if (not is_enabled):
      is_enabled=True
      #print_date=tgt_altaz[i].obstime.strftime('%Y-%m-%d %H:%M:%S')
      print_date=x_dates[i].strftime('%Y-%m-%d %H:%M')
      print(f"start {print_date} az:{tgt_altaz[i].az.value:.2f} alt:{tgt_altaz[i].alt.value:.2f}")
      print_time=x_dates[i].strftime('%H:%M')
      all_intervals.append(f"S {print_time}")
  else:
    if (is_enabled):
      is_enabled=False
      #print_date=tgt_altaz[i].obstime.strftime('%Y-%m-%d %H:%M:%S')
      print_date=x_dates[i].strftime('%Y-%m-%d %H:%M')
      print(f"stop  {print_date} az:{tgt_altaz[i].az.value:.2f} alt:{tgt_altaz[i].alt.value:.2f}")
      print("")
      print_time=x_dates[i].strftime('%H:%M')
      all_intervals.append(f"E {print_time}")

#finished with observaton enable - stop it
if is_enabled:
  #print the last available observation
  i=len(tgt_altaz)-1
  print_date=x_dates[i].strftime('%Y-%m-%d %H:%M')
  print(f"stop  {print_date} az:{tgt_altaz[i].az.value:.2f} alt:{tgt_altaz[i].alt.value:.2f}")
  print_time=x_dates[i].strftime('%H:%M')
  all_intervals.append(f"E {print_time}")

plt.xlabel('Time of day [HH:MM]')
ax_az.set(ylabel = 'Azimuth [deg]')
ax_alt.set(ylabel = 'Altitude [deg]')

# beautify the x-labels
plt.gcf().autofmt_xdate(which='both')

plt.savefig('obs_plan_'+tgt_name+'.png', dpi=150)

#if the available window goes over star/end of the 24h used for calculations
if (all_intervals[0].lstrip('SE ') == all_intervals[-1].lstrip('SE ')):
    #remove the start/end at now()
    all_intervals.pop(0)
    all_intervals.pop(-1)
    #move first end after last start
    all_intervals.append(all_intervals.pop(0))

for i in all_intervals:
    if (i[0] == 'S'):
        print(f"  {i[2:]} - ", end='', file=sys.stderr)
    else:
        print(f"{i[2:]}", end='', file=sys.stderr)
print('',file=sys.stderr)
