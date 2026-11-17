"use client";
import { SensorType } from "@/types/Sensor";
import { se } from "date-fns/locale";
import { useState } from "react";

export const SelectSensorComponent = ({
  sensors,
  updateSelectedSensor,
  initialSelectedSensor,
}: {
  sensors: SensorType[];
  updateSelectedSensor: (sensorId: string) => void;
  initialSelectedSensor: string;
}) => {
  const [selectIsOpen, setSelectIsOpen] = useState(false);
  const [selectValue, setSelectValue] = useState(initialSelectedSensor);
  return (
    <div className="flex flex-col gap-2 w-full">
      <label htmlFor="sensor" className="text-foreground">
        Valitse sensori
      </label>
      <div className="relative w-full text-start text-foreground cursor-pointer border border-border p-2 rounded-xl outline-none hover:ring-1 hover:ring-highlight focus-within:ring-1 focus-within:ring-highlight transition-all duration-100 ease-in-out flex gap-2 items-center justify-between">
        <select
          onFocus={() => setSelectIsOpen(true)}
          onBlur={() => setSelectIsOpen(false)}
          onChange={(e) => {
            setSelectIsOpen(false);
            setSelectValue(e.target.value);
            updateSelectedSensor(e.target.value);
          }}
          onMouseDown={() => setSelectIsOpen((prev) => !prev)}
          name="sensor"
          id="sensor"
          value={selectValue}
          className="appearance-none outline-none w-full"
        >
          {sensors.map((sensor) => (
            <option
              className="bg-background text-foreground"
              key={sensor.sensorId}
              value={sensor.sensorId}
            >{`${sensor.sensorName} (#${sensor.sensorId})`}</option>
          ))}
        </select>
        <svg
          xmlns="http://www.w3.org/2000/svg"
          fill="none"
          viewBox="0 0 24 24"
          strokeWidth={1.5}
          stroke="currentColor"
          className={`size-5 transition-all pointer-events-none absolute right-2 ${selectIsOpen && "-rotate-180"}`}
        >
          <path
            strokeLinecap="round"
            strokeLinejoin="round"
            d="m4.5 5.25 7.5 7.5 7.5-7.5m-15 6 7.5 7.5 7.5-7.5"
          />
        </svg>
      </div>
    </div>
  );
};
