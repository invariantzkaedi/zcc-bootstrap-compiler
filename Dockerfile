FROM python:3.11-slim
WORKDIR /app
COPY requirements.txt .
RUN pip install --no-cache-dir -r requirements.txt
COPY app ./app
COPY artifacts ./artifacts
ENV PYTHONUNBUFFERED=1
CMD ["uvicorn", "app.fastapi_retrieval_gateway:app", "--host", "0.0.0.0", "--port", "8080"]