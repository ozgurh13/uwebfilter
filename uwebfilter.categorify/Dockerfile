FROM python:3.12.4-slim-bullseye

WORKDIR /app

COPY train.py inference.py server.py requirements /app/
RUN pip install -r requirements

ENTRYPOINT ["python", "server.py"]
