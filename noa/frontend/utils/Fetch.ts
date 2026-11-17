import { getMeasurementsFromSensor } from "@/api/Measurements";
import { MeasurementType } from "../types/MeasurementCache";

export const getMoreMeasurements = async ({
  from,
  to,
  sensorId,
}: {
  from: number;
  to: number;
  sensorId: string;
}) => {
  const fetchedMeasurements: MeasurementType[] =
    await getMeasurementsFromSensor({
      from: from,
      to: to,
      sensorId: sensorId,
    });
  return fetchedMeasurements;
};
