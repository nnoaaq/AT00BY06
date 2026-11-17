"use client";

import { SensorType } from "@/types/Sensor";
import { SelectSensorComponent } from "./SelectSensor";
import { useEffect, useState } from "react";
import { MeasurementCache, MeasurementType } from "../types/MeasurementCache";
import { ChartComponent2 } from "./Chart";
import { getMeasurementsFromSensor } from "@/api/Measurements";
import { convertDateToUnix, getCurrentDayUnixRange } from "../utils/Time";
import { SelectDateComponent } from "./SelectDate";
import { da } from "date-fns/locale";
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
      // TARKISTETAAN ONKO SENSORILLA OLLENKAAN MITTAUKSIA CACHESSA
      const sensorAlreadyInCache = measurementsCache.get(selectedSensor);
      if (!sensorAlreadyInCache) {
        const currentDayUnixRange = getCurrentDayUnixRange();
        const fetchedMeasurements: MeasurementType[] =
          await getMeasurementsFromSensor({
            ...currentDayUnixRange,
            sensorId: selectedSensor,
          });
        if (!fetchedMeasurements?.length) return;
        // PÄIVITETÄÄN CACHEA UUSILLA MITTAUKSILLA
        setMeasurementsCache((prevCache) => {
          const map = new Map(prevCache);
          const measurementsAlreadyInCache = map.get(selectedSensor) || {
            measurements: new Map<number, MeasurementType>(),
            cachedDays: new Set<string>(),
          };
          fetchedMeasurements.forEach((measurement) => {
            measurementsAlreadyInCache.measurements.set(
              measurement.timeStamp,
              measurement,
            );
          });
          // DATE-OLIO HAETTUJEN POHJALTA (HAETAAN AINA TÄYSI VUOROKAUSI)
          const dateObj = new Date(fetchedMeasurements[0].timeStamp * 1000);
          measurementsAlreadyInCache.cachedDays.add(
            dateObj.toLocaleDateString("fi-FI"),
          );
          map.set(selectedSensor, measurementsAlreadyInCache);
          return map;
        });
        return;
      }
      // TARKISTETAAN ONKO SENSORILLA OIKEA PÄIVÄ
      if (selectedDateRange) {
        // TARKISTETAAN MITÄ PÄIVIÄ ON VÄLILLÄ
        const datesBetweenUnixTimestamps =
          getDatesBetweenUnixTimestamps(selectedDateRange);
        for (let date of datesBetweenUnixTimestamps) {
          const dateAlreadyInCache = sensorAlreadyInCache.cachedDays.has(date);
          if (!dateAlreadyInCache) {
            const unixSeconds = convertDateToUnix(date);
            const fetchedMeasurements = await getMoreMeasurements({
              ...unixSeconds,
              sensorId: selectedSensor,
            });
            if (!fetchedMeasurements?.length) continue;
            // PÄIVITETÄÄN CACHEA UUSILLA MITTAUKSILLA
            setMeasurementsCache((prevCache) => {
              const map = new Map(prevCache);
              const measurementsAlreadyInCache = map.get(selectedSensor) || {
                measurements: new Map<number, MeasurementType>(),
                cachedDays: new Set<string>(),
              };
              fetchedMeasurements.forEach((measurement) => {
                measurementsAlreadyInCache.measurements.set(
                  measurement.timeStamp,
                  measurement,
                );
              });
              // DATE-OLIO HAETTUJEN POHJALTA (HAETAAN AINA TÄYSI VUOROKAUSI)
              const dateObj = new Date(fetchedMeasurements[0].timeStamp * 1000);
              measurementsAlreadyInCache.cachedDays.add(
                dateObj.toLocaleDateString("fi-FI"),
              );
              map.set(selectedSensor, measurementsAlreadyInCache);
              return map;
            });
          }
        }
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
