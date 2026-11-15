"use client";
import { useEffect, useState } from "react";
import { SensorType } from "../types/Sensor";
import { SelectComponent } from "./Select";
import { MeasurementType } from "../types/Measurement";
import { ChartComponent } from "./Chart";
import { getUnixTime } from "../utils/Date";
import { getMeasurementsFromSensor } from "../api/Measurements";
import { DatePicker } from "./DatePicker";
export const SensorController = ({
  sensors,
  initialMeasurements,
}: {
  sensors: SensorType[];
  initialMeasurements: MeasurementType[];
}) => {
  // USE_STATET
  const [selectedSensor, setSelectedSensor] = useState<string>(
    sensors[0].sensorId,
  );
  const [selectedDays, setSelectedDays] = useState<{
    from: number;
    to: number;
  }>(getUnixTime());
  // MAPPI MEASUREMENTEILLE
  // ALUSTETAAN AUTOMAATTISESTI HAETULLA DATALLA
  const [measurements, setMeasurements] = useState<
    Map<string, MeasurementType[]>
  >(() => {
    const map = new Map();
    initialMeasurements.forEach((measurement) => {
      const measurementsAlreadyInMap = map.get(measurement.sensorId) || [];
      const sortedMeasurements = [
        ...measurementsAlreadyInMap,
        measurement,
      ].sort((a, b) => a.timeStamp - b.timeStamp);

      map.set(measurement.sensorId, sortedMeasurements);
    });
    return map;
  });
  // SUODATETUT MITTAUKSET
  const filteredMeasurements = measurements
    .get(selectedSensor)
    ?.filter((measurement) => {
      if (
        measurement.timeStamp > selectedDays.from &&
        measurement.timeStamp < selectedDays.to
      )
        return measurement;
    })
    .sort((a, b) => a.timeStamp - b.timeStamp);
  // USE_EFFECTIT
  useEffect(() => {
    const fetch = async () => {
      const fetchedMeasurements: MeasurementType[] =
        await getMeasurementsFromSensor({
          ...selectedDays,
          sensorId: selectedSensor,
        });
      if (!fetchedMeasurements || fetchedMeasurements.length === 0) return;
      setMeasurements((previousMeasurements) => {
        const map = new Map(previousMeasurements);
        fetchedMeasurements.forEach((measurement) => {
          const measurementsAlreadyInMap = map.get(measurement.sensorId) || [];
          const alreadyInCache = measurementsAlreadyInMap.some(
            (m) => m.timeStamp === measurement.timeStamp,
          );
          if (!alreadyInCache) {
            map.set(measurement.sensorId, [
              ...measurementsAlreadyInMap,
              measurement,
            ]);
          }
        });
        return map;
      });
    };
    fetch();
  }, [selectedSensor, selectedDays]);
  return (
    <div>
      <DatePicker
        reset={selectedSensor}
        updateSelectedDays={(
          days: { from: number; to: number } | undefined,
        ) => {
          if (!days) return;
          setSelectedDays(days);
        }}
      />
      {sensors && (
        <SelectComponent
          changeSelectedSensor={(sensorId: string) => {
            setSelectedSensor(sensorId);
            setSelectedDays(getUnixTime());
          }}
          selectedSensor={selectedSensor}
          sensors={sensors}
        />
      )}
      {measurements && (
        <div>
          <ChartComponent
            chartType="temperature"
            measurements={filteredMeasurements || []}
          />

          <ChartComponent
            chartType="humidity"
            measurements={filteredMeasurements || []}
          />
        </div>
      )}
    </div>
  );
};
