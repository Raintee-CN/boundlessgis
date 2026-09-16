FROM node:22-alpine AS web-builder

WORKDIR /web
COPY web/package.json web/package-lock.json ./
RUN npm ci
COPY web/ ./
RUN npm run build

FROM postgis/postgis:18-3.6

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
      gdal-bin \
      python3-pip \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY service/requirements.txt /app/requirements.txt
RUN pip3 install --break-system-packages --no-cache-dir -r /app/requirements.txt
COPY service/app.py /app/app.py
COPY --from=web-builder /web/dist /app/static

EXPOSE 8080
CMD ["uvicorn", "app:app", "--host", "0.0.0.0", "--port", "8080"]
