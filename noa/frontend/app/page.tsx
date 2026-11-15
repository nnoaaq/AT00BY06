import { Suspense } from "react";
import { Loader } from "@/components/Loader";

export default async function Home() {
  return (
    <div className="p-2">
      <Suspense fallback={<p>Sensorit latautuvat edelleen...</p>}>
        <Loader />
      </Suspense>
    </div>
  );
}
