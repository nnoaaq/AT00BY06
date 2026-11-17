"use client";
import {
  Chart as ChartJS,
  ArcElement,
  Tooltip,
  Legend,
  CategoryScale,
  LinearScale,
  LineElement,
  PointElement,
  scales,
} from "chart.js";
import { Line } from "react-chartjs-2";
import { convertUnixTimeToDay } from "../utils/Time";
import { MeasurementType } from "../types/MeasurementCache";
ChartJS.register(
  ArcElement,
  Tooltip,
  Legend,
  CategoryScale,
  LinearScale,
  LineElement,
  PointElement,
);
export const ChartComponent2 = ({
  measurements,
  chartType,
}: {
  measurements: MeasurementType[];
  chartType: "temperature" | "humidity";
}) => {
  const isTemperatureChart = chartType === "temperature";
  const convertTailwindColor = (variableName: string) => {
    if (typeof window === "undefined") return "";
    return getComputedStyle(document.documentElement)
      .getPropertyValue(variableName)
      .trim();
  };
  const chartColor = isTemperatureChart
    ? convertTailwindColor("--color-temperature")
    : convertTailwindColor("--color-humidity");
  const chartData = {
    labels: measurements.map((measurement) =>
      convertUnixTimeToDay(measurement.timeStamp),
    ),
    datasets: [
      {
        label: isTemperatureChart ? "Lämpötila" : "Kosteus",
        data: measurements.map((measurement) => measurement[chartType]),
        fill: false,
        borderColor: chartColor,
        backgroundColor: chartColor,
        tension: 0.1,
        pointRadius: 0,
        pointHoverRadius: 6,
        pointHitRadius: 25,
      },
    ],
  };

  const chartOptions = {
    responsive: true,
    maintainAspectRatio: false,
    plugins: {
      legend: {
        position: "bottom" as const,
      },
    },
    scales: {
      x: {
        ticks: {
          autoSkip: true,
          maxTicksLimit: 5,
        },
      },
    },
  };
  return (
    <div className="w-full h-100">
      <Line options={chartOptions} data={chartData} />
    </div>
  );
};
