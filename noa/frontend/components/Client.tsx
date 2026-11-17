"use client";

import { SensorType } from "@/types/Sensor";
import { SelectSensorComponent } from "./SelectSensor";
import { useEffect, useState } from "react";
import { MeasurementCache, MeasurementType } from "../types/MeasurementCache";
import { ChartComponent2 } from "./Chart";
import { getCurrentDayUnixRange } from "../utils/Time";
import { SelectDateComponent } from "./SelectDate";
import { getDatesBetweenUnixTimestamps } from "../utils/Date";
import { getMoreMeasurements } from "../utils/Fetch";

export const ClientComponent = ({
  sensors,
  initialMeasurements,
  initialSelectedSensor,
}: {
  sensors: SensorType[];
  initialMeasurements: MeasurementType[];
  initialSelectedSensor: string;
}) => {
  // USE STATET
  const [selectedSensor, setSelecterSensor] = useState(initialSelectedSensor);
  const [selectedDateRange, setSelectedDateRange] = useState<{
    from: number;
    to: number;
  } | null>(null);
  const [measurementsCache, setMeasurementsCache] = useState(() => {
    const map = new Map<string, MeasurementCache>();
    const timestampMap = new Map<number, MeasurementType>();
    initialMeasurements.forEach((measurement) => {
      timestampMap.set(measurement.timeStamp, measurement);
    });
    const initialMeasurementsDateObj = new Date(
      initialMeasurements[0].timeStamp * 1000,
    );
    map.set(initialSelectedSensor, {
      measurements: timestampMap,
      cachedDays: new Set([
        initialMeasurementsDateObj.toLocaleDateString("fi-FI"),
      ]),
    });
    return map;
  });
  // MITTAUSTEN HAKEMINEN SENSORIN TAI PÄIVÄMÄÄRÄN MUUTTUESSA
  useEffect(() => {
    const fetch = async () => {
      let timeRange;
      if (!selectedDateRange) {
        timeRange = getCurrentDayUnixRange();
      } else {
        timeRange = selectedDateRange;
      }
      const datesToShow = getDatesBetweenUnixTimestamps(timeRange);
      // HAETAAN SENSORIN MITTAUKSET CACHESTA TAI LUODAAN TYHJÄ RAKENNE
      const measurementsInCache = measurementsCache.get(selectedSensor) || {
        measurements: new Map<number, MeasurementType>(),
        cachedDays: new Set<string>(),
      };
      // TARKISTETAAN, ONKO TIETOJA TALLENNETTU KYSEISELTÄ PÄIVÄLTÄ
      // JOS YKSIKIN PÄIVÄ PUUTTUU, HAETAAN KOKO ANNETTU AIKAVÄLI
      // VAIKKA HAETTAISIINKIN TURHASTI PÄIVIÄ
      let missingDays = false;
      for (let dateToShow of datesToShow) {
        const dateAlreadyInCache =
          measurementsInCache.cachedDays.has(dateToShow);
        if (!dateAlreadyInCache) {
          missingDays = true;
        }
      }
      // AJANJAKSO TARKISTETTU
      if (missingDays) {
        const fetchedMeasurements: MeasurementType[] =
          await getMoreMeasurements({
            ...timeRange,
            sensorId: selectedSensor,
          });
        if (!fetchedMeasurements.length) return;
        // MITTAUSTULOKSIA ON TULLUT
        setMeasurementsCache((prevCache) => {
          const map = new Map(prevCache);
          const measurementsInCache = measurementsCache.get(selectedSensor) || {
            measurements: new Map<number, MeasurementType>(),
            cachedDays: new Set<string>(),
          };
          fetchedMeasurements.forEach((measurement) => {
            measurementsInCache.measurements.set(
              measurement.timeStamp,
              measurement,
            );
          });
          datesToShow.forEach((date) => {
            measurementsInCache.cachedDays.add(date);
          });
          map.set(selectedSensor, measurementsInCache);
          return map;
        });
      }
    };
    fetch();
  }, [selectedSensor, selectedDateRange]);
  useEffect(() => {
    setSelectedDateRange(null);
  }, [selectedSensor]);
  // NÄYTETTÄVIEN MITTAUSTEN SUODATUS
  const dateToday = new Date().toLocaleDateString("fi-FI");
  const filteredMeasurements = Array.from(
    measurementsCache.get(selectedSensor)?.measurements.values() ?? [],
  )
    ?.filter((measurement) => {
      if (!selectedDateRange) {
        return (
          new Date(measurement.timeStamp * 1000).toLocaleDateString("fi-FI") ===
          dateToday
        );
      }
      return (
        measurement.timeStamp >= selectedDateRange.from &&
        measurement.timeStamp <= selectedDateRange.to
      );
    })
    .sort((a, b) => a.timeStamp - b.timeStamp);
  return (
    <div className="p-2 border border-border-dark rounded-xl shadow-lg flex flex-col gap-5">
      <div className="flex flex-col md:flex-row gap-2">
        <SelectSensorComponent
          initialSelectedSensor={initialSelectedSensor}
          sensors={sensors}
          updateSelectedSensor={(sensorId: string) => {
            setSelecterSensor(sensorId);
          }}
        />
        <SelectDateComponent
          reset={selectedSensor}
          updateSelectedDays={(days) => {
            if (!days) return;
            setSelectedDateRange(days);
          }}
        />
      </div>
      <ChartComponent2
        measurements={filteredMeasurements}
        chartType="temperature"
      />
      <ChartComponent2
        measurements={filteredMeasurements}
        chartType="humidity"
      />
    </div>
  );
};
