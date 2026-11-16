"use client";

import { SensorType } from "@/types/Sensor";

export const SelectComponent = ({
  sensors,
  selectedSensor,
  changeSelectedSensor,
}: {
  sensors: SensorType[];
  selectedSensor: string;
  changeSelectedSensor: (sensorId: string) => void;
}) => {
  // LÄHETÄ TIETO VAIHDETUSTA SENSORISTA YLÖSPÄIN
  const handleChange = (sensorId: string) => {
    changeSelectedSensor(sensorId);
  };

  return (
    <div className="">
      <div className="flex flex-col gap-2 ">
        <label htmlFor="sensors" className="text-zinc-100">
          Valitse sensori
        </label>
        <div className="w-full  text-start text-zinc-100 cursor-pointer border border-zinc-600 p-2 rounded-xl outline-none hover:ring-1 hover:ring-amber-500 focus:ring-1 focus:ring-amber-500 transition-all duration-100 ease-in-out flex gap-2 items-center justify-between">
          <select
            defaultValue={selectedSensor}
            onChange={(e) => handleChange(e.target.value)}
            name="sensors"
            id="sensors"
            className="appearance-none w-full"
          >
            {sensors.map((sensor) => (
              <option
                value={sensor.sensorId}
                key={sensor.sensorId}
              >{`${sensor.sensorName} #${sensor.sensorId}`}</option>
            ))}
          </select>
          <svg
            xmlns="http://www.w3.org/2000/svg"
            fill="none"
            viewBox="0 0 24 24"
            strokeWidth={1.5}
            stroke="currentColor"
            className="size-4"
          >
            <path
              strokeLinecap="round"
              strokeLinejoin="round"
              d="m19.5 8.25-7.5 7.5-7.5-7.5"
            />
          </svg>
        </div>
      </div>
    </div>
  );
};
