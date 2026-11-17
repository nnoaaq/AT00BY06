export type MeasurementType = {
  sensorId: string;
  sensorName: string;
  timeStamp: number;
  temperature?: number;
  humidity?: number;
};
export type MeasurementCache = {
  measurements: Map<number, MeasurementType>;
  cachedDays: Set<string>;
};
