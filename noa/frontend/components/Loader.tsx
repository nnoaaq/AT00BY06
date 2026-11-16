import { connection } from "next/server";
import { notFound } from "next/navigation";
import { MeasurementType } from "@/types/Measurement";
import { SensorType } from "@/types/Sensor";
import { fetchSensors } from "@/api/Sensors";
import { getUnixTime } from "@/utils/Date";
import { getMeasurementsFromSensor } from "@/api/Measurements";
import { SensorController } from "./SensorController";

export const Loader = async () => {
  // HOITAA TIETOKANTA HAUT
  await connection(); // EI PRERENDERÖINTIÄ
  const sensors: SensorType[] = await fetchSensors();
  const unixDay: { from: number; to: number } = getUnixTime();
  // HAETAAN NYKYINEN PÄIVÄ ENSIMMÄISELTÄ LÖYTYNEELTÄ SENSORILTA
  const initialMeasurements: MeasurementType[] =
    await getMeasurementsFromSensor({
      ...unixDay,
      sensorId: sensors[0].sensorId,
    });
  if (
    !sensors ||
    sensors.length === 0 ||
    !initialMeasurements ||
    initialMeasurements.length === 0
  ) {
    notFound();
  }
  return (
    <div>
      <SensorController
        sensors={sensors}
        initialMeasurements={initialMeasurements}
      />
    </div>
  );
};
