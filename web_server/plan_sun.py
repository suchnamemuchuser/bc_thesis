import sqlite3
import datetime as dt
import numpy as np

import astropy.units as u
from astropy.coordinates import AltAz, EarthLocation, SkyCoord, get_body
from astropy.time import Time

# Silence astropy download logs
from astropy.utils.data import conf
conf.remote_timeout = 30
conf.show_progress_bar = False

AZ_MIN, AZ_MAX = 29, 355
ALT_MIN, ALT_MAX = 15, 84
ONDREJOV = EarthLocation(lat=49.9085742*u.deg, lon=14.7797511*u.deg, height=512*u.m)


def main():

    today_str = dt.datetime.now().strftime("%Y-%m-%d")
    obs_time = Time(today_str)

    delta_t = np.linspace(0, 24, 24*60+1) * u.hour
    times = obs_time + delta_t

    sun_path = get_body("Sun", times, ONDREJOV)

    altaz = sun_path.transform_to(AltAz(obstime=times, location=ONDREJOV))
    visible = (altaz.az.value > AZ_MIN) & (altaz.az.value < AZ_MAX) & \
              (altaz.alt.value > ALT_MIN) & (altaz.alt.value < ALT_MAX)

    if np.any(visible):
        # Find all indices where the sun is within your AZ/ALT limits
        indices = np.where(visible)[0]
        
        # Get the start and end Time objects from our 'times' array
        start_rec = times[indices[0]].unix
        end_rec = times[indices[-1]].unix

        start_ts = int(start_rec)
        end_ts = int(end_rec)

        print(f"Planning Sun from {start_ts} to {end_ts} for {dt.datetime.today().strftime('%Y-%m-%d')}")

        conn = sqlite3.connect('plan.db')
        cursor = conn.cursor()

        query = """
        SELECT 1 FROM plan 
        WHERE (start_time > ?) AND (start_time < ?) or (end_time > ?) and (end_time < ?)
        LIMIT 1;
        """

        cursor.execute(query, (start_ts, end_ts, start_ts, end_ts))
        conflict = cursor.fetchone()

        if not conflict:
            cursor.execute(
                "INSERT INTO plan (object_name, is_interstellar, start_time, end_time) VALUES (?, ?, ?, ?)",
                ("Sun", 0, start_ts, end_ts)
            )
            conn.commit()
            print("Sun successfully added to plan.")
        else:
            print("Conflict: A recording is already planned during this solar window.")

        conn.close()

if __name__ == "__main__":
    main()